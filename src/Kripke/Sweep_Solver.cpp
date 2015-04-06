/*--------------------------------------------------------------------------
 * Sweep-based solver routine.
 *--------------------------------------------------------------------------*/

#include <Kripke.h>
#include <Kripke/SweepComm.h>
#include <Kripke/Grid.h>
#include <vector>
#include <stdio.h>


/**
  Run solver iterations.
*/
int SweepSolver (Grid_Data *grid_data)
{
  Kernel *kernel = grid_data->kernel;

  BLOCK_TIMER(grid_data->timing, Solve);

  // Loop over iterations
  for(int iter = 0;iter < grid_data->niter;++ iter){

    /*
     * Compute the RHS
     */

    // Discrete to Moments transformation
    {
      BLOCK_TIMER(grid_data->timing, LTimes);
      kernel->LTimes(grid_data);
    }


    // Compute Scattering Source Term
    {
      BLOCK_TIMER(grid_data->timing, Scattering);
      kernel->scattering(grid_data);
    }


    // Moments to Discrete transformation
    {
      BLOCK_TIMER(grid_data->timing, LPlusTimes);
      kernel->LPlusTimes(grid_data);
    }

    /*
     * Sweep each Group Set
     */
    {
      BLOCK_TIMER(grid_data->timing, Sweep);

      if(true){
        // Create a list of all groups
        std::vector<int> sdom_list(grid_data->subdomains.size());
        for(int i = 0;i < grid_data->subdomains.size();++ i){
          sdom_list[i] = i;
        }

        // Sweep everything
        SweepSubdomains(sdom_list, grid_data);
      }
      // This is the ARDRA version, doing each groupset sweep independently
      else{
        for(int group_set = 0;group_set < grid_data->num_group_sets;++ group_set){
          std::vector<int> sdom_list;
          // Add all subdomains for this groupset
          for(int s = 0;s < grid_data->subdomains.size();++ s){
            if(grid_data->subdomains[s].idx_group_set == group_set){
              sdom_list.push_back(s);
            }
          }

          // Sweep the groupset
          SweepSubdomains(sdom_list, grid_data);
        }
      }
    }
  }
  return(0);
}



/**
  Perform full parallel sweep algorithm on subset of subdomains.
*/
int SweepSubdomains (std::vector<int> subdomain_list, Grid_Data *grid_data)
{
  // Create a new sweep communicator object
  SweepComm sweep_comm;

  // Add all subdomains in our list
  for(int i = 0;i < subdomain_list.size();++ i){
    int sdom_id = subdomain_list[i];
    sweep_comm.addSubdomain(sdom_id, grid_data->subdomains[sdom_id]);
  }

  /* Loop until we have finished all of our work */
  while(sweep_comm.workRemaining()){

    // Get a list of subdomains that have met dependencies
    std::vector<int> sdom_ready = sweep_comm.readySubdomains();

    for(int idx = 0;idx < sdom_ready.size();++ idx){
      int sdom_id = sdom_ready[idx];

      /* Use standard Diamond-Difference sweep */
      {
        BLOCK_TIMER(grid_data->timing, Sweep_Kernel);

        Subdomain &sdom = grid_data->subdomains[sdom_id];
        grid_data->kernel->sweep(&sdom);
      }

      // Mark as complete (and do any communication)
      sweep_comm.markComplete(sdom_id);
    }
  }

  return(0);
}


