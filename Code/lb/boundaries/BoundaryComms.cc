// 
// Copyright (C) University College London, 2007-2012, all rights reserved.
// 
// This file is part of HemeLB and is CONFIDENTIAL. You may not work 
// with, install, use, duplicate, modify, redistribute or share this
// file, or any part thereof, other than as allowed by any agreement
// specifically made by you with University College London.
// 

#include "lb/boundaries/BoundaryComms.h"
#include "lb/boundaries/BoundaryValues.h"
#include "topology/NetworkTopology.h"
#include "util/utilityFunctions.h"

namespace hemelb
{
  namespace lb
  {
    namespace boundaries
    {

      BoundaryComms::BoundaryComms(SimulationState* iSimState, std::vector<int> &iProcsList, bool iHasBoundary) :
          hasBoundary(iHasBoundary), nProcs((int) iProcsList.size()), procsList(iProcsList)
      {
        /* iProcsList contains the procs containing said Boundary/iolet, but NOT proc 0 (the BoundaryControlling/BC proc)! */

        // Only BC proc sends
        if (BoundaryValues::IsCurrentProcTheBCProc())
        {
          sendRequest = new MPI_Request[nProcs];
          sendStatus = new MPI_Status[nProcs];
        }
        else
        {
          sendRequest = NULL;
          sendStatus = NULL;
        }
      }

      BoundaryComms::~BoundaryComms()
      {

        if (BoundaryValues::IsCurrentProcTheBCProc())
        {
          delete[] sendRequest;
          delete[] sendStatus;
        }
      }

      void BoundaryComms::Wait()
      {
        if (hasBoundary)
        {
          MPI_Wait(&receiveRequest, &receiveStatus);
        }
      }

      void BoundaryComms::WaitAllComms()
      {
        // Now wait for all to complete
        if (BoundaryValues::IsCurrentProcTheBCProc())
        {
          MPI_Waitall(nProcs, sendRequest, sendStatus);

          if (hasBoundary)
            MPI_Wait(&receiveRequest, &receiveStatus);
        }
        else
        {
          MPI_Wait(&receiveRequest, &receiveStatus);
        }

      }

      // It is up to the caller to make sure only BCproc calls send
      void BoundaryComms::Send(distribn_t* density)
      {
        for (int proc = 0; proc < nProcs; proc++)
        {
          MPI_Isend(density,
                    1,
                    hemelb::MpiDataType(*density),
                    procsList[proc],
                    100,
                    MPI_COMM_WORLD,
                    &sendRequest[proc]);
        }
      }

      void BoundaryComms::Receive(distribn_t* density)
      {
        if (hasBoundary)
        {
          MPI_Irecv(density,
                    1,
                    hemelb::MpiDataType(*density),
                    BoundaryValues::GetBCProcRank(),
                    100,
                    MPI_COMM_WORLD,
                    &receiveRequest);
        }
      }

      void BoundaryComms::SendDoubles(double* double_array, int size)
      {
        for (int proc = 0; proc < nProcs; proc++)
        {
          hemelb::log::Logger::Log<hemelb::log::Debug, hemelb::log::OnePerCore>("SendDoubles() to rank = %i, double array is %e %e %e, size %i",
                                                                               procsList[proc],
                                                                               double_array[0],
                                                                               double_array[1],
                                                                               double_array[2],
                                                                               size);
          MPI_Isend(double_array, size, MPI_DOUBLE, procsList[proc], 101, MPI_COMM_WORLD, &sendRequest[proc]);

        }
        MPI_Waitall(nProcs, sendRequest, sendStatus);
      }

      void BoundaryComms::ReceiveDoubles(double* double_array, int size)
      {
        hemelb::log::Logger::Log<hemelb::log::Debug, hemelb::log::OnePerCore>("RecvDoubles() bcprocrank = %i, double array is %e %e %e",
                                                                             BoundaryValues::GetBCProcRank(),
                                                                             double_array[0],
                                                                             double_array[1],
                                                                             double_array[2]);

        MPI_Irecv(double_array,
                               size,
                               MPI_DOUBLE,
                               BoundaryValues::GetBCProcRank(),
                               101,
                               MPI_COMM_WORLD,
                               &receiveRequest);


        MPI_Wait(&receiveRequest, &receiveStatus);
        hemelb::log::Logger::Log<hemelb::log::Debug, hemelb::log::OnePerCore>("After recv: bcprocrank = %i, double array is %e %e %e",
                                                                                     BoundaryValues::GetBCProcRank(),
                                                                                     double_array[0],
                                                                                     double_array[1],
                                                                                     double_array[2]);
        //hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("End of ReceiveDoubles()");
      }

      void BoundaryComms::FinishSend()
      {
        // Don't move on to next step with BC proc until all messages have been sent
        // Precautionary measure to make sure proc doesn't overwrite, before message is sent
        if (BoundaryValues::IsCurrentProcTheBCProc())
        {
          MPI_Waitall(nProcs, sendRequest, sendStatus);
        }
      }

    }
  }
}
