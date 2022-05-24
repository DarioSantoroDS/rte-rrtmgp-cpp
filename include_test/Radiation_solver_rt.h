/*
 * This file is imported from MicroHH (https://github.com/earth-system-radiation/earth-system-radiation)
 * and is adapted for the testing of the C++ interface to the
 * RTE+RRTMGP radiation code.
 *
 * MicroHH is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MicroHH is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MicroHH.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RADIATION_SOLVER_RT_H
#define RADIATION_SOLVER_RT_H

#include "Array.h"
#include "Gas_concs.h"
#include "Gas_optics_rrtmgp_rt.h" 
#include "Cloud_optics_rt.h"
#include "Rte_lw_rt.h"
#include "Rte_sw_rt.h"
#include "Raytracer.h"
#include "raytracer_kernels.h"
#include "Source_functions_rt.h"
#include <curand_kernel.h>


class Radiation_solver_longwave
{
    public:

        Radiation_solver_longwave(
                const Gas_concs_gpu& gas_concs,
                const std::string& file_name_gas,
                const std::string& file_name_cloud);

        #ifdef __CUDACC__
        void solve_gpu(
                const bool switch_fluxes,
                const bool switch_cloud_optics,
                const bool switch_output_optical,
                const bool switch_output_bnd_fluxes,
                const Gas_concs_gpu& gas_concs,
                const Array_gpu<Float,2>& p_lay, const Array_gpu<Float,2>& p_lev,
                const Array_gpu<Float,2>& t_lay, const Array_gpu<Float,2>& t_lev,
                Array_gpu<Float,2>& col_dry,
                const Array_gpu<Float,1>& t_sfc, const Array_gpu<Float,2>& emis_sfc,
                const Array_gpu<Float,2>& lwp, const Array_gpu<Float,2>& iwp,
                const Array_gpu<Float,2>& rel, const Array_gpu<Float,2>& rei,
                Array_gpu<Float,3>& tau, Array_gpu<Float,3>& lay_source,
                Array_gpu<Float,3>& lev_source_inc, Array_gpu<Float,3>& lev_source_dec, Array_gpu<Float,2>& sfc_source,
                Array_gpu<Float,2>& lw_flux_up, Array_gpu<Float,2>& lw_flux_dn, Array_gpu<Float,2>& lw_flux_net,
                Array_gpu<Float,3>& lw_bnd_flux_up, Array_gpu<Float,3>& lw_bnd_flux_dn, Array_gpu<Float,3>& lw_bnd_flux_net);

        int get_n_gpt_gpu() const { return this->kdist_gpu->get_ngpt(); };
        int get_n_bnd_gpu() const { return this->kdist_gpu->get_nband(); };
        
        Array<int,2> get_band_lims_gpoint_gpu() const
        { return this->kdist_gpu->get_band_lims_gpoint(); }

        Array<Float,2> get_band_lims_wavenumber_gpu() const
        { return this->kdist_gpu->get_band_lims_wavenumber(); }
        #endif

    private:
        #ifdef __CUDACC__
        std::unique_ptr<Gas_optics_rrtmgp_rt> kdist_gpu;
        std::unique_ptr<Cloud_optics_rt> cloud_optics_gpu;
        Rte_lw_rt rte_lw;

        std::unique_ptr<Optical_props_arry_rt> optical_props;

        std::unique_ptr<Source_func_lw_rt> sources;

        std::unique_ptr<Optical_props_1scl_rt> cloud_optical_props;
        #endif
};

class Radiation_solver_shortwave
{
    public:
        Radiation_solver_shortwave(
                const Gas_concs_gpu& gas_concs,
                const std::string& file_name_gas,
                const std::string& file_name_cloud);

        #ifdef __CUDACC__
        void solve_gpu(
                const bool switch_fluxes,
                const bool switch_raytracing,
                const bool switch_cloud_optics,
                const bool switch_output_optical,
                const bool switch_output_bnd_fluxes,
                const Int ray_count,
                const Gas_concs_gpu& gas_concs,
                const Array_gpu<Float,2>& p_lay, const Array_gpu<Float,2>& p_lev,
                const Array_gpu<Float,2>& t_lay, const Array_gpu<Float,2>& t_lev,
                const Array_gpu<Float,1>& z_lev,
                const Array_gpu<Float,1>& grid_dims,
                Array_gpu<Float,2>& col_dry,
                const Array_gpu<Float,2>& sfc_alb_dir, const Array_gpu<Float,2>& sfc_alb_dif,
                const Array_gpu<Float,1>& tsi_scaling, const Array_gpu<Float,1>& mu0,
                const Array_gpu<Float,2>& lwp, const Array_gpu<Float,2>& iwp,
                const Array_gpu<Float,2>& rel, const Array_gpu<Float,2>& rei,
                Array_gpu<Float,3>& tau, Array_gpu<Float,3>& ssa, Array_gpu<Float,3>& g,
                Array_gpu<Float,2>& toa_src,
                Array_gpu<Float,2>& sw_flux_up, Array_gpu<Float,2>& sw_flux_dn,
                Array_gpu<Float,2>& sw_flux_dn_dir, Array_gpu<Float,2>& sw_flux_net,
                Array_gpu<Float,3>& sw_bnd_flux_up, Array_gpu<Float,3>& sw_bnd_flux_dn,
                Array_gpu<Float,3>& sw_bnd_flux_dn_dir, Array_gpu<Float,3>& sw_bnd_flux_net,
                Array_gpu<Float,2>& rt_flux_tod_up,
                Array_gpu<Float,2>& rt_flux_sfc_dir,
                Array_gpu<Float,2>& rt_flux_sfc_dif,
                Array_gpu<Float,2>& rt_flux_sfc_up,
                Array_gpu<Float,3>& rt_flux_abs_dir,
                Array_gpu<Float,3>& rt_flux_abs_dif);

        int get_n_gpt_gpu() const { return this->kdist_gpu->get_ngpt(); };
        int get_n_bnd_gpu() const { return this->kdist_gpu->get_nband(); };

        Float get_tsi_gpu() const { return this->kdist_gpu->get_tsi(); };
        
        Array<int,2> get_band_lims_gpoint_gpu() const
        { return this->kdist_gpu->get_band_lims_gpoint(); }

        Array<Float,2> get_band_lims_wavenumber_gpu() const
        { return this->kdist_gpu->get_band_lims_wavenumber(); }
        #endif

    private:
        #ifdef __CUDACC__
        std::unique_ptr<Gas_optics_rt> kdist_gpu;
        std::unique_ptr<Cloud_optics_rt> cloud_optics_gpu;
        Rte_sw_rt rte_sw;
        Raytracer raytracer;

        std::unique_ptr<Optical_props_arry_rt> optical_props;

        std::unique_ptr<Optical_props_2str_rt> cloud_optical_props;
        #endif
};
#endif