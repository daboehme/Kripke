#include<Kripke/Kernel/Kernel_3d_GZD.h>
#include<Kripke/Grid.h>
#include<Kripke/SubTVec.h>

Kernel_3d_GZD::Kernel_3d_GZD() {

}

Kernel_3d_GZD::~Kernel_3d_GZD() {

}

Nesting_Order Kernel_3d_GZD::nestingPsi(void) const {
  return NEST_GZD;
}

Nesting_Order Kernel_3d_GZD::nestingPhi(void) const {
  return NEST_GZD;
}

Nesting_Order Kernel_3d_GZD::nestingSigt(void) const {
  return NEST_DGZ;
}

Nesting_Order Kernel_3d_GZD::nestingEll(void) const {
  return NEST_ZDG;
}

Nesting_Order Kernel_3d_GZD::nestingEllPlus(void) const {
  return NEST_ZDG;
}

void Kernel_3d_GZD::LTimes(Grid_Data *grid_data) {
  // Outer parameters
  int nidx = grid_data->total_num_moments;

  // Clear phi
  for(int ds = 0;ds < grid_data->num_zone_sets;++ ds){
    grid_data->phi[ds]->clear(0.0);
  }

 // Loop over Subdomains
  int num_subdomains = grid_data->subdomains.size();
  for (int sdom_id = 0; sdom_id < num_subdomains; ++ sdom_id){
    Subdomain &sdom = grid_data->subdomains[sdom_id];

    // Get dimensioning
    int num_zones = sdom.num_zones;
    int num_local_groups = sdom.num_groups;
    int group0 = sdom.group0;
    int num_local_directions = sdom.num_directions;
    int num_groups_zones = num_local_groups*num_zones;

    /* 3D Cartesian Geometry */
    double *ell_ptr = sdom.ell->ptr();

#ifdef KRIPKE_USE_OPENMP
#pragma omp parallel for
#endif
    for(int gz = 0;gz < num_groups_zones; ++ gz){
      double * KRESTRICT psi = sdom.psi->ptr() + gz*num_local_directions;
      double * KRESTRICT phi = sdom.phi->ptr(group0, 0, 0) + gz*nidx;
      double * KRESTRICT ell_d = ell_ptr;

      for (int d = 0; d < num_local_directions; d++) {
        double psi_d = psi[d];

        for(int nm_offset = 0;nm_offset < nidx;++nm_offset){
          phi[nm_offset] += ell_d[nm_offset] * psi_d;
        }
        ell_d += nidx;
      }

    }
  } // Subdomain
}

void Kernel_3d_GZD::LPlusTimes(Grid_Data *grid_data) {
  // Outer parameters
  int num_moments = grid_data->num_legendre;
  int nidx = grid_data->total_num_moments;

  // Loop over Subdomains
  int num_subdomains = grid_data->subdomains.size();
  for (int sdom_id = 0; sdom_id < num_subdomains; ++ sdom_id){
    Subdomain &sdom = grid_data->subdomains[sdom_id];

    // Get dimensioning
    int num_zones = sdom.num_zones;
    int num_local_groups = sdom.num_groups;
    int group0 = sdom.group0;
    int num_local_directions = sdom.num_directions;
    int num_groups_zones = num_local_groups*num_zones;

    sdom.rhs->clear(0.0);

    /* 3D Cartesian Geometry */
    double * KRESTRICT ell_plus_ptr = sdom.ell_plus->ptr();

#ifdef KRIPKE_USE_OPENMP
#pragma omp parallel for
#endif
    for(int gz = 0;gz < num_groups_zones; ++ gz){
      double * KRESTRICT rhs = sdom.rhs->ptr(0, 0, 0) + gz*num_local_directions;
      double * KRESTRICT phi_out = sdom.phi_out->ptr(group0, 0, 0) + gz*nidx;
      double * KRESTRICT ell_plus_d = ell_plus_ptr;

      for (int d = 0; d < num_local_directions; d++) {

        for(int nm_offset = 0;nm_offset < nidx;++nm_offset){
          rhs[d] += ell_plus_d[nm_offset] * phi_out[nm_offset];
        }
        ell_plus_d += nidx;
      }
    }
  } // Subdomain
}


/**
  Compute scattering source term phi_out from flux moments in phi.
*/
void Kernel_3d_GZD::scattering(Grid_Data *grid_data){

}

/* Sweep routine for Diamond-Difference */
/* Macros for offsets with fluxes on cell faces */
#define I_PLANE_INDEX(j, k) (k)*(local_jmax) + (j)
#define J_PLANE_INDEX(i, k) (k)*(local_imax) + (i)
#define K_PLANE_INDEX(i, j) (j)*(local_imax) + (i)
#define Zonal_INDEX(i, j, k) (i) + (local_imax)*(j) \
  + (local_imax)*(local_jmax)*(k)

void Kernel_3d_GZD::sweep(Subdomain *sdom) {
  int num_directions = sdom->num_directions;
  int num_groups = sdom->num_groups;
  int num_zones = sdom->num_zones;

  Directions *direction = sdom->directions;

  int local_imax = sdom->nzones[0];
  int local_jmax = sdom->nzones[1];
  int local_kmax = sdom->nzones[2];

  double *dx = &sdom->deltas[0][0];
  double *dy = &sdom->deltas[1][0];
  double *dz = &sdom->deltas[2][0];

  // Upwind/Downwind face flux data
  SubTVec &i_plane = *sdom->plane_data[0];
  SubTVec &j_plane = *sdom->plane_data[1];
  SubTVec &k_plane = *sdom->plane_data[2];

  // All directions have same id,jd,kd, since these are all one Direction Set
  // So pull that information out now
  Grid_Sweep_Block const &extent = sdom->sweep_block;

#ifdef KRIPKE_USE_OPENMP
#pragma omp parallel for
#endif
  for (int group = 0; group < num_groups; ++group) {
    double *sigt_g = sdom->sigt->ptr(group, 0, 0);

    /*  Perform transport sweep of the grid 1 cell at a time.   */
    for (int k = extent.start_k; k != extent.end_k; k += extent.inc_k) {
      double dzk = dz[k + 1];
      double two_dz = 2.0 / dzk;
      for (int j = extent.start_j; j != extent.end_j; j += extent.inc_j) {
        double dyj = dy[j + 1];
        double two_dy = 2.0 / dyj;
        for (int i = extent.start_i; i != extent.end_i; i += extent.inc_i) {
          double dxi = dx[i + 1];
          double two_dx = 2.0 / dxi;

          int z = Zonal_INDEX(i, j, k);

          double * KRESTRICT psi_g_z = sdom->psi->ptr(group, 0, z);
          double * KRESTRICT rhs_g_z = sdom->rhs->ptr(group, 0, z);

          double * KRESTRICT psi_lf_g_z = i_plane.ptr(group, 0, I_PLANE_INDEX(j, k));
          double * KRESTRICT psi_fr_g_z = j_plane.ptr(group, 0, J_PLANE_INDEX(i, k));
          double * KRESTRICT psi_bo_g_z = k_plane.ptr(group, 0, K_PLANE_INDEX(i, j));

          for (int d = 0; d < num_directions; ++d) {
            double xcos = direction[d].xcos;
            double ycos = direction[d].ycos;
            double zcos = direction[d].zcos;

            double zcos_dzk = zcos * two_dz;
            double ycos_dyj = ycos * two_dy;
            double xcos_dxi = xcos * two_dx;

            /* Calculate new zonal flux */
            double psi_g_z_d = (rhs_g_z[d] + psi_lf_g_z[d] * xcos_dxi
                + psi_fr_g_z[d] * ycos_dyj + psi_bo_g_z[d] * zcos_dzk)
                / (xcos_dxi + ycos_dyj + zcos_dzk
                    + sigt_g[Zonal_INDEX(i, j, k)]);

            psi_g_z[d] = psi_g_z_d;

            /* Apply diamond-difference relationships */
            psi_lf_g_z[d] = 2.0 * psi_g_z_d - psi_lf_g_z[d];
            psi_fr_g_z[d] = 2.0 * psi_g_z_d - psi_fr_g_z[d];
            psi_bo_g_z[d] = 2.0 * psi_g_z_d - psi_bo_g_z[d];
          }
        }
      }
    }
  } // group
}

