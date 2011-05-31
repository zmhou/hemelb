#include <vector>
#include <math.h>
#include <limits>

#include "util/utilityFunctions.h"
#include "vis/Control.h"
#include "vis/RayTracer.h"
#include "vis/GlyphDrawer.h"

#include "io/XdrFileWriter.h"

namespace hemelb
{
  namespace vis
  {
    Control::Control(lb::StressTypes iStressType,
                     net::Net* net,
                     lb::SimulationState* simState,
                     geometry::LatticeData* iLatDat) :
      net::PhasedBroadcastRegular<true, 2, 0, false, true>(net, simState, SPREADFACTOR)
    {
      mVisSettings.mStressType = iStressType;

      this->vis = new Vis;

      //sites_x etc are globals declared in net.h
      vis->half_dim[0] = 0.5F * float (iLatDat->GetXSiteCount());
      vis->half_dim[1] = 0.5F * float (iLatDat->GetYSiteCount());
      vis->half_dim[2] = 0.5F * float (iLatDat->GetZSiteCount());

      vis->system_size = 2.F * fmaxf(vis->half_dim[0], fmaxf(vis->half_dim[1], vis->half_dim[2]));

      mVisSettings.mouse_x = -1;
      mVisSettings.mouse_y = -1;

      initLayers(iLatDat);
    }

    void Control::initLayers(geometry::LatticeData* iLatDat)
    {
      // We don't have all the minima / maxima on one core, so we have to gather them.
      // NOTE this only happens once, during initialisation, otherwise it would be
      // totally unforgivable.
      site_t block_min_x = std::numeric_limits<site_t>::max();
      site_t block_min_y = std::numeric_limits<site_t>::max();
      site_t block_min_z = std::numeric_limits<site_t>::max();
      site_t block_max_x = std::numeric_limits<site_t>::min();
      site_t block_max_y = std::numeric_limits<site_t>::min();
      site_t block_max_z = std::numeric_limits<site_t>::min();

      site_t n = -1;

      for (site_t i = 0; i < iLatDat->GetXBlockCount(); i++)
      {
        for (site_t j = 0; j < iLatDat->GetYBlockCount(); j++)
        {
          for (site_t k = 0; k < iLatDat->GetZBlockCount(); k++)
          {
            n++;

            geometry::LatticeData::BlockData * lBlock = iLatDat->GetBlock(n);
            if (lBlock->ProcessorRankForEachBlockSite == NULL)
            {
              continue;
            }

            block_min_x = util::NumericalFunctions::min(block_min_x, i);
            block_min_y = util::NumericalFunctions::min(block_min_y, j);
            block_min_z = util::NumericalFunctions::min(block_min_z, k);
            block_max_x = util::NumericalFunctions::max(block_max_x, i);
            block_max_y = util::NumericalFunctions::max(block_max_y, j);
            block_max_z = util::NumericalFunctions::max(block_max_z, k);
          }
        }
      }

      site_t mins[3], maxes[3];
      site_t localMins[3], localMaxes[3];

      localMins[0] = block_min_x;
      localMins[1] = block_min_y;
      localMins[2] = block_min_z;

      localMaxes[0] = block_max_x;
      localMaxes[1] = block_max_y;
      localMaxes[2] = block_max_z;

      MPI_Allreduce(localMins, mins, 3, MpiDataType<site_t> (), MPI_MIN, MPI_COMM_WORLD);
      MPI_Allreduce(localMaxes, maxes, 3, MpiDataType<site_t> (), MPI_MAX, MPI_COMM_WORLD);

      mVisSettings.ctr_x = 0.5F * (float) (iLatDat->GetBlockSize() * (mins[0] + maxes[0]));
      mVisSettings.ctr_y = 0.5F * (float) (iLatDat->GetBlockSize() * (mins[1] + maxes[1]));
      mVisSettings.ctr_z = 0.5F * (float) (iLatDat->GetBlockSize() * (mins[2] + maxes[2]));

      myRayTracer = new RayTracer(iLatDat, &mDomainStats, &mScreen, &mViewpoint, &mVisSettings);
      myGlypher = new GlyphDrawer(iLatDat, &mScreen, &mDomainStats, &mViewpoint, &mVisSettings);

#ifndef NO_STREAKLINES
      myStreaker = new StreaklineDrawer(iLatDat, &mScreen, &mViewpoint, &mVisSettings);
#endif
      // Note that rtInit does stuff to this->ctr_x (because this has
      // to be global)
      mVisSettings.ctr_x -= vis->half_dim[0];
      mVisSettings.ctr_y -= vis->half_dim[1];
      mVisSettings.ctr_z -= vis->half_dim[2];
    }

    void Control::SetProjection(const int &iPixels_x,
                                const int &iPixels_y,
                                const float &iLocal_ctr_x,
                                const float &iLocal_ctr_y,
                                const float &iLocal_ctr_z,
                                const float &iLongitude,
                                const float &iLatitude,
                                const float &iZoom)
    {
      float rad = 5.F * vis->system_size;
      float dist = 0.5F * rad;

      float centre[3] = { iLocal_ctr_x, iLocal_ctr_y, iLocal_ctr_z };

      mViewpoint.SetViewpointPosition(iLongitude * (float) DEG_TO_RAD, iLatitude
          * (float) DEG_TO_RAD, centre, rad, dist);

      mScreen.Set( (0.5F * vis->system_size) / iZoom,
                   (0.5F * vis->system_size) / iZoom,
                  iPixels_x,
                  iPixels_y,
                  rad,
                  &mViewpoint);
    }

    void Control::RegisterSite(site_t i, distribn_t density, distribn_t velocity, distribn_t stress)
    {
      myRayTracer->UpdateClusterVoxel(i, density, velocity, stress);
    }

    void Control::SetSomeParams(const float iBrightness,
                                const distribn_t iDensityThresholdMin,
                                const distribn_t iDensityThresholdMinMaxInv,
                                const distribn_t iVelocityThresholdMaxInv,
                                const distribn_t iStressThresholdMaxInv)
    {
      mVisSettings.brightness = iBrightness;
      mDomainStats.density_threshold_min = iDensityThresholdMin;

      mDomainStats.density_threshold_minmax_inv = iDensityThresholdMinMaxInv;
      mDomainStats.velocity_threshold_max_inv = iVelocityThresholdMaxInv;
      mDomainStats.stress_threshold_max_inv = iStressThresholdMaxInv;
    }

    void Control::UpdateImageSize(int pixels_x, int pixels_y)
    {
      mScreen.Resize(pixels_x, pixels_y);
    }

    void Control::Render(geometry::LatticeData* iLatDat)
    {
      mScreen.Reset();

      myRayTracer->Render();

      if (mVisSettings.mode == VisSettings::ISOSURFACESANDGLYPHS)
      {
        myGlypher->Render();
      }
#ifndef NO_STREAKLINES
      if (mVisSettings.mStressType == lb::ShearStress || mVisSettings.mode
          == VisSettings::WALLANDSTREAKLINES)
      {
        myStreaker->render(iLatDat);
      }
#endif

      CompositeImage();
    }

    void Control::CompositeImage()
    {
      // Status object for MPI comms.
      MPI_Status status;

      /*
       * We do several iterations.
       *
       * On the first, every even proc passes data to the odd proc below, where it is merged.
       * On the second, the difference is two, so proc 3 passes to 1, 7 to 5, 11 to 9 etc.
       * On the third the differenec is four, so proc 5 passes to 1, 13 to 9 etc.
       * .
       * .
       * .
       *
       * This continues until all data is passed back to processor one, which passes it to proc 0.
       */
      topology::NetworkTopology* netTop = topology::NetworkTopology::Instance();

      // Start with a difference in rank of 1, doubling every time.
      for (proc_t deltaRank = 1; deltaRank < netTop->GetProcessorCount(); deltaRank <<= 1)
      {
        // The receiving proc is all the ranks that are 1 modulo (deltaRank * 2)
        for (proc_t receivingProc = 1; receivingProc < (netTop->GetProcessorCount() - deltaRank); receivingProc
            += deltaRank << 1)
        {
          proc_t sendingProc = receivingProc + deltaRank;

          // If we're the sending proc, do the send.
          if (netTop->GetLocalRank() == sendingProc)
          {
            MPI_Send(&mScreen.pixels.pixelCount,
                     1,
                     MpiDataType(mScreen.pixels.pixelCount),
                     receivingProc,
                     20,
                     MPI_COMM_WORLD);

            if (mScreen.pixels.pixelCount > 0)
            {
              MPI_Send(mScreen.pixels.pixels,
                       mScreen.pixels.pixelCount,
                       MpiDataType<ColPixel> (),
                       receivingProc,
                       20,
                       MPI_COMM_WORLD);
            }
          }

          // If we're the receiving proc, receive.
          else if (netTop->GetLocalRank() == receivingProc)
          {
            MPI_Recv(&recvBuffers.pixelCount,
                     1,
                     MpiDataType(recvBuffers.pixelCount),
                     sendingProc,
                     20,
                     MPI_COMM_WORLD,
                     &status);

            if (recvBuffers.pixelCount > 0)
            {
              MPI_Recv(recvBuffers.pixels,
                       recvBuffers.pixelCount,
                       MpiDataType<ColPixel> (),
                       sendingProc,
                       20,
                       MPI_COMM_WORLD,
                       &status);

              mScreen.pixels.FoldIn(&recvBuffers, &mVisSettings);
            }
          }
        }
      }

      // Send the final image from proc 1 to 0.
      if (netTop->GetLocalRank() == 1)
      {
        MPI_Send(&mScreen.pixels.pixelCount,
                 1,
                 MpiDataType(mScreen.pixels.pixelCount),
                 0,
                 20,
                 MPI_COMM_WORLD);

        if (mScreen.pixels.pixelCount > 0)
        {
          MPI_Send(mScreen.pixels.pixels,
                   mScreen.pixels.pixelCount,
                   MpiDataType<ColPixel> (),
                   0,
                   20,
                   MPI_COMM_WORLD);
        }

      }
      // Receive the final image on proc 0.
      else if (netTop->GetLocalRank() == 0)
      {
        MPI_Recv(&mScreen.pixels.pixelCount,
                 1,
                 MpiDataType(mScreen.pixels.pixelCount),
                 1,
                 20,
                 MPI_COMM_WORLD,
                 &status);

        if (mScreen.pixels.pixelCount > 0)
        {
          MPI_Recv(mScreen.pixels.pixels,
                   mScreen.pixels.pixelCount,
                   MpiDataType<ColPixel> (),
                   1,
                   20,
                   MPI_COMM_WORLD,
                   &status);
        }
      }

      mScreen.pixelCountInBuffer = mScreen.pixels.pixelCount;

      for (unsigned int m = 0; m < mScreen.pixelCountInBuffer; m++)
      {
        mScreen.pixels.pixelId[mScreen.pixels.pixels[m].GetI() * mScreen.GetPixelsY()
            + mScreen.pixels.pixels[m].GetJ()] = -1;
      }
    }

    void Control::SetMouseParams(double iPhysicalPressure, double iPhysicalStress)
    {
      mVisSettings.mouse_pressure = iPhysicalPressure;
      mVisSettings.mouse_stress = iPhysicalStress;
    }

    bool Control::MouseIsOverPixel(float* density, float* stress)
    {
      if (mVisSettings.mouse_x < 0 || mVisSettings.mouse_y < 0)
      {
        return false;
      }

      return mScreen.MouseIsOverPixel(mVisSettings.mouse_x, mVisSettings.mouse_y, density, stress);
    }

    void Control::ProgressStreaklines(unsigned long time_step,
                                      unsigned long period,
                                      geometry::LatticeData* iLatDat)
    {
      myStreaker ->StreakLines(time_step, period, iLatDat);
    }

    void Control::Reset()
    {
      myStreaker->Restart();

      base::Reset();
    }

    Control::~Control()
    {
#ifndef NO_STREAKLINES
      delete myStreaker;
#endif

      delete vis;
      delete myGlypher;
      delete myRayTracer;
    }

  } // namespace vis
} // namespace hemelb
