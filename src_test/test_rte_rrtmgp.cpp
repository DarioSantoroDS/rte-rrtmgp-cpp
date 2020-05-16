/*
 * This file is part of a C++ interface to the Radiative Transfer for Energetics (RTE)
 * and Rapid Radiative Transfer Model for GCM applications Parallel (RRTMGP).
 *
 * The original code is found at https://github.com/RobertPincus/rte-rrtmgp.
 *
 * Contacts: Robert Pincus and Eli Mlawer
 * email: rrtmgp@aer.com
 *
 * Copyright 2015-2020,  Atmospheric and Environmental Research and
 * Regents of the University of Colorado.  All right reserved.
 *
 * This C++ interface can be downloaded from https://github.com/microhh/rte-rrtmgp-cpp
 *
 * Contact: Chiel van Heerwaarden
 * email: chiel.vanheerwaarden@wur.nl
 *
 * Copyright 2020, Wageningen University & Research.
 *
 * Use and duplication is permitted under the terms of the
 * BSD 3-clause license, see http://opensource.org/licenses/BSD-3-Clause
 *
 */

#include <boost/algorithm/string.hpp>
#include <cmath>

#include "Netcdf_interface.h"
#include "Array.h"
#include "Gas_concs.h"
#include "Gas_optics_rrtmgp.h"
#include "Optical_props.h"
#include "Source_functions.h"
#include "Fluxes.h"
#include "Rte_lw.h"
#include "Rte_sw.h"

#ifdef FLOAT_SINGLE_RRTMGP
#define FLOAT_TYPE float
#else
#define FLOAT_TYPE double
#endif

namespace
{
    std::vector<std::string> get_variable_string(
            const std::string& var_name,
            std::vector<int> i_count,
            Netcdf_handle& input_nc,
            const int string_len,
            bool trim=true)
    {
        // Multiply all elements in i_count.
        int total_count = std::accumulate(i_count.begin(), i_count.end(), 1, std::multiplies<>());

        // Add the string length as the rightmost dimension.
        i_count.push_back(string_len);

        // Read the entire char array;
        std::vector<char> var_char;
        var_char = input_nc.get_variable<char>(var_name, i_count);

        std::vector<std::string> var;

        for (int n=0; n<total_count; ++n)
        {
            std::string s(var_char.begin()+n*string_len, var_char.begin()+(n+1)*string_len);
            if (trim)
                boost::trim(s);
            var.push_back(s);
        }

        return var;
    }

    template<typename TF>
    Gas_optics_rrtmgp<TF> load_and_init_gas_optics(
            Master& master,
            const Gas_concs<TF>& gas_concs,
            const std::string& coef_file)
    {
        // READ THE COEFFICIENTS FOR THE OPTICAL SOLVER.
        Netcdf_file coef_nc(master, coef_file, Netcdf_mode::Read);

        // Read k-distribution information.
        int n_temps = coef_nc.get_dimension_size("temperature");
        int n_press = coef_nc.get_dimension_size("pressure");
        int n_absorbers = coef_nc.get_dimension_size("absorber");

        // CvH: I hardcode the value to 32 now, because coef files
        // CvH: changed dimension name inconsistently.
        // int n_char = coef_nc.get_dimension_size("string_len");
        constexpr int n_char = 32;

        int n_minorabsorbers = coef_nc.get_dimension_size("minor_absorber");
        int n_extabsorbers = coef_nc.get_dimension_size("absorber_ext");
        int n_mixingfracs = coef_nc.get_dimension_size("mixing_fraction");
        int n_layers = coef_nc.get_dimension_size("atmos_layer");
        int n_bnds = coef_nc.get_dimension_size("bnd");
        int n_gpts = coef_nc.get_dimension_size("gpt");
        int n_pairs = coef_nc.get_dimension_size("pair");
        int n_minor_absorber_intervals_lower = coef_nc.get_dimension_size("minor_absorber_intervals_lower");
        int n_minor_absorber_intervals_upper = coef_nc.get_dimension_size("minor_absorber_intervals_upper");
        int n_contributors_lower = coef_nc.get_dimension_size("contributors_lower");
        int n_contributors_upper = coef_nc.get_dimension_size("contributors_upper");

        // Read gas names.
        Array<std::string,1> gas_names(
                get_variable_string("gas_names", {n_absorbers}, coef_nc, n_char, true), {n_absorbers});

        Array<int,3> key_species(
                coef_nc.get_variable<int>("key_species", {n_bnds, n_layers, 2}),
                {2, n_layers, n_bnds});
        Array<TF,2> band_lims(coef_nc.get_variable<TF>("bnd_limits_wavenumber", {n_bnds, 2}), {2, n_bnds});
        Array<int,2> band2gpt(coef_nc.get_variable<int>("bnd_limits_gpt", {n_bnds, 2}), {2, n_bnds});
        Array<TF,1> press_ref(coef_nc.get_variable<TF>("press_ref", {n_press}), {n_press});
        Array<TF,1> temp_ref(coef_nc.get_variable<TF>("temp_ref", {n_temps}), {n_temps});

        TF temp_ref_p = coef_nc.get_variable<TF>("absorption_coefficient_ref_P");
        TF temp_ref_t = coef_nc.get_variable<TF>("absorption_coefficient_ref_T");
        TF press_ref_trop = coef_nc.get_variable<TF>("press_ref_trop");

        Array<TF,3> kminor_lower(
                coef_nc.get_variable<TF>("kminor_lower", {n_temps, n_mixingfracs, n_contributors_lower}),
                {n_contributors_lower, n_mixingfracs, n_temps});
        Array<TF,3> kminor_upper(
                coef_nc.get_variable<TF>("kminor_upper", {n_temps, n_mixingfracs, n_contributors_upper}),
                {n_contributors_upper, n_mixingfracs, n_temps});

        Array<std::string,1> gas_minor(get_variable_string("gas_minor", {n_minorabsorbers}, coef_nc, n_char),
                {n_minorabsorbers});

        Array<std::string,1> identifier_minor(
                get_variable_string("identifier_minor", {n_minorabsorbers}, coef_nc, n_char), {n_minorabsorbers});

        Array<std::string,1> minor_gases_lower(
                get_variable_string("minor_gases_lower", {n_minor_absorber_intervals_lower}, coef_nc, n_char),
                {n_minor_absorber_intervals_lower});
        Array<std::string,1> minor_gases_upper(
                get_variable_string("minor_gases_upper", {n_minor_absorber_intervals_upper}, coef_nc, n_char),
                {n_minor_absorber_intervals_upper});

        Array<int,2> minor_limits_gpt_lower(
                coef_nc.get_variable<int>("minor_limits_gpt_lower", {n_minor_absorber_intervals_lower, n_pairs}),
                {n_pairs, n_minor_absorber_intervals_lower});
        Array<int,2> minor_limits_gpt_upper(
                coef_nc.get_variable<int>("minor_limits_gpt_upper", {n_minor_absorber_intervals_upper, n_pairs}),
                {n_pairs, n_minor_absorber_intervals_upper});

        Array<BOOL_TYPE,1> minor_scales_with_density_lower(
                coef_nc.get_variable<BOOL_TYPE>("minor_scales_with_density_lower", {n_minor_absorber_intervals_lower}),
                {n_minor_absorber_intervals_lower});
        Array<BOOL_TYPE,1> minor_scales_with_density_upper(
                coef_nc.get_variable<BOOL_TYPE>("minor_scales_with_density_upper", {n_minor_absorber_intervals_upper}),
                {n_minor_absorber_intervals_upper});

        Array<BOOL_TYPE,1> scale_by_complement_lower(
                coef_nc.get_variable<BOOL_TYPE>("scale_by_complement_lower", {n_minor_absorber_intervals_lower}),
                {n_minor_absorber_intervals_lower});
        Array<BOOL_TYPE,1> scale_by_complement_upper(
                coef_nc.get_variable<BOOL_TYPE>("scale_by_complement_upper", {n_minor_absorber_intervals_upper}),
                {n_minor_absorber_intervals_upper});

        Array<std::string,1> scaling_gas_lower(
                get_variable_string("scaling_gas_lower", {n_minor_absorber_intervals_lower}, coef_nc, n_char),
                {n_minor_absorber_intervals_lower});
        Array<std::string,1> scaling_gas_upper(
                get_variable_string("scaling_gas_upper", {n_minor_absorber_intervals_upper}, coef_nc, n_char),
                {n_minor_absorber_intervals_upper});

        Array<int,1> kminor_start_lower(
                coef_nc.get_variable<int>("kminor_start_lower", {n_minor_absorber_intervals_lower}),
                {n_minor_absorber_intervals_lower});
        Array<int,1> kminor_start_upper(
                coef_nc.get_variable<int>("kminor_start_upper", {n_minor_absorber_intervals_upper}),
                {n_minor_absorber_intervals_upper});

        Array<TF,3> vmr_ref(
                coef_nc.get_variable<TF>("vmr_ref", {n_temps, n_extabsorbers, n_layers}),
                {n_layers, n_extabsorbers, n_temps});

        Array<TF,4> kmajor(
                coef_nc.get_variable<TF>("kmajor", {n_temps, n_press+1, n_mixingfracs, n_gpts}),
                {n_gpts, n_mixingfracs, n_press+1, n_temps});

        // Keep the size at zero, if it does not exist.
        Array<TF,3> rayl_lower;
        Array<TF,3> rayl_upper;

        if (coef_nc.variable_exists("rayl_lower"))
        {
            rayl_lower.set_dims({n_gpts, n_mixingfracs, n_temps});
            rayl_upper.set_dims({n_gpts, n_mixingfracs, n_temps});
            rayl_lower = coef_nc.get_variable<TF>("rayl_lower", {n_temps, n_mixingfracs, n_gpts});
            rayl_upper = coef_nc.get_variable<TF>("rayl_upper", {n_temps, n_mixingfracs, n_gpts});
        }

        // Is it really LW if so read these variables as well.
        if (coef_nc.variable_exists("totplnk"))
        {
            int n_internal_sourcetemps = coef_nc.get_dimension_size("temperature_Planck");

            Array<TF,2> totplnk(
                    coef_nc.get_variable<TF>( "totplnk", {n_bnds, n_internal_sourcetemps}),
                    {n_internal_sourcetemps, n_bnds});
            Array<TF,4> planck_frac(
                    coef_nc.get_variable<TF>("plank_fraction", {n_temps, n_press+1, n_mixingfracs, n_gpts}),
                    {n_gpts, n_mixingfracs, n_press+1, n_temps});

            // Construct the k-distribution.
            return Gas_optics_rrtmgp<TF>(
                    gas_concs,
                    gas_names,
                    key_species,
                    band2gpt,
                    band_lims,
                    press_ref,
                    press_ref_trop,
                    temp_ref,
                    temp_ref_p,
                    temp_ref_t,
                    vmr_ref,
                    kmajor,
                    kminor_lower,
                    kminor_upper,
                    gas_minor,
                    identifier_minor,
                    minor_gases_lower,
                    minor_gases_upper,
                    minor_limits_gpt_lower,
                    minor_limits_gpt_upper,
                    minor_scales_with_density_lower,
                    minor_scales_with_density_upper,
                    scaling_gas_lower,
                    scaling_gas_upper,
                    scale_by_complement_lower,
                    scale_by_complement_upper,
                    kminor_start_lower,
                    kminor_start_upper,
                    totplnk,
                    planck_frac,
                    rayl_lower,
                    rayl_upper);
        }
        else
        {
            Array<TF,1> solar_src_quiet(
                    coef_nc.get_variable<TF>("solar_source_quiet", {n_gpts}), {n_gpts});
            Array<TF,1> solar_src_facular(
                    coef_nc.get_variable<TF>("solar_source_facular", {n_gpts}), {n_gpts});
            Array<TF,1> solar_src_sunspot(
                    coef_nc.get_variable<TF>("solar_source_sunspot", {n_gpts}), {n_gpts});

            TF tsi = coef_nc.get_variable<TF>("tsi_default");
            TF mg_index = coef_nc.get_variable<TF>("mg_default");
            TF sb_index = coef_nc.get_variable<TF>("sb_default");

            return Gas_optics_rrtmgp<TF>(
                    gas_concs,
                    gas_names,
                    key_species,
                    band2gpt,
                    band_lims,
                    press_ref,
                    press_ref_trop,
                    temp_ref,
                    temp_ref_p,
                    temp_ref_t,
                    vmr_ref,
                    kmajor,
                    kminor_lower,
                    kminor_upper,
                    gas_minor,
                    identifier_minor,
                    minor_gases_lower,
                    minor_gases_upper,
                    minor_limits_gpt_lower,
                    minor_limits_gpt_upper,
                    minor_scales_with_density_lower,
                    minor_scales_with_density_upper,
                    scaling_gas_lower,
                    scaling_gas_upper,
                    scale_by_complement_lower,
                    scale_by_complement_upper,
                    kminor_start_lower,
                    kminor_start_upper,
                    solar_src_quiet,
                    solar_src_facular,
                    solar_src_sunspot,
                    tsi,
                    mg_index,
                    sb_index,
                    rayl_lower,
                    rayl_upper);
        }
        // End reading of k-distribution.
    }
}

template<typename TF>
void solve_radiation(Master& master)
{
    Netcdf_file input_nc(master, "rte_rrtmgp_input.nc", Netcdf_mode::Read);

    ////// READ THE ATMOSPHERIC DATA //////
    int n_lay = input_nc.get_dimension_size("lay");
    int n_lev = input_nc.get_dimension_size("lev");
    int n_col = input_nc.get_dimension_size("col");

    Array<TF,2> p_lay(input_nc.get_variable<TF>("lay", {n_lay, n_col}), {n_col, n_lay});
    Array<TF,2> t_lay(input_nc.get_variable<TF>("t_lay", {n_lay, n_col}), {n_col, n_lay});
    Array<TF,2> p_lev(input_nc.get_variable<TF>("lev", {n_lev, n_col}), {n_col, n_lev});
    Array<TF,2> t_lev(input_nc.get_variable<TF>("t_lev", {n_lev, n_col}), {n_col, n_lev});

    const BOOL_TYPE top_at_1 = p_lay({1, 1}) < p_lay({1, n_lay});

    Gas_concs<TF> gas_concs;
    Gas_concs<TF> gas_concs_subset;

    gas_concs.set_vmr("h2o",
            Array<TF,2>(input_nc.get_variable<TF>("vmr_h2o", {n_lay, n_col}), {n_col, n_lay}));
    gas_concs.set_vmr("co2",
            Array<TF,2>(input_nc.get_variable<TF>("vmr_co2", {n_lay, n_col}), {n_col, n_lay}));
    gas_concs.set_vmr("o3",
            Array<TF,2>(input_nc.get_variable<TF>("vmr_o3", {n_lay, n_col}), {n_col, n_lay}));
    gas_concs.set_vmr("n2o",
            Array<TF,2>(input_nc.get_variable<TF>("vmr_n2o", {n_lay, n_col}), {n_col, n_lay}));
    // gas_concs.set_vmr("co",
    //         Array<TF,2>(input_nc.get_variable<TF>("vmr_co", {n_lay, n_col}), {n_col, n_lay}));
    gas_concs.set_vmr("ch4",
            Array<TF,2>(input_nc.get_variable<TF>("vmr_ch4", {n_lay, n_col}), {n_col, n_lay}));
    gas_concs.set_vmr("o2",
            Array<TF,2>(input_nc.get_variable<TF>("vmr_o2", {n_lay, n_col}), {n_col, n_lay}));
    // gas_concs.set_vmr("n2",
    //         Array<TF,2>(input_nc.get_variable<TF>("vmr_n2", {n_lay, n_col}), {n_col, n_lay}));

    // Construct the gas optics classes for the solvers.
    std::unique_ptr<Gas_optics_rrtmgp<TF>> kdist_lw = std::make_unique<Gas_optics_rrtmgp<TF>>(
        load_and_init_gas_optics(master, gas_concs, "coefficients_lw.nc"));

    // Fetch the col_dry in case present.
    Array<TF,2> col_dry({n_col, n_lay});
    if (input_nc.variable_exists("col_dry"))
        col_dry = input_nc.get_variable<TF>("col_dry", {n_lay, n_col});
    else
        Gas_optics_rrtmgp<TF>::get_col_dry(col_dry, gas_concs.get_vmr("h2o"), p_lev);


    ////// READ THE BOUNDARY CONDITIONS //////
    const int n_gpt = kdist_lw->get_ngpt();
    const int n_bnd = kdist_lw->get_nband();

    // Boundary conditions for longwave.
    Array<TF,2> emis_sfc(
            input_nc.get_variable<TF>(
                    "emis_sfc", {n_col, n_bnd}), {n_bnd, n_col});
    Array<TF,1> t_sfc(
            input_nc.get_variable<TF>(
                    "t_sfc", {n_col}), {n_col});


    ////// CREATE THE OUTPUT ARRAYS THAT NEED TO BE STORED //////
    Array<TF,2> lw_flux_up ({n_col, n_lev});
    Array<TF,2> lw_flux_dn ({n_col, n_lev});
    Array<TF,2> lw_flux_net({n_col, n_lev});

    Array<TF,3> lw_bnd_flux_up ({n_col, n_lev, n_bnd});
    Array<TF,3> lw_bnd_flux_dn ({n_col, n_lev, n_bnd});
    Array<TF,3> lw_bnd_flux_net({n_col, n_lev, n_bnd});


    ////// SOLVING THE LONGWAVE RADIATION //////
    master.print_message("Solving the radiation.\n");

    constexpr int n_col_block = 4;

    // Read the sources and create containers for the substeps.
    int n_blocks = n_col / n_col_block;
    int n_col_block_left = n_col % n_col_block;

    std::unique_ptr<Optical_props_arry<TF>> optical_props_subset;
    std::unique_ptr<Optical_props_arry<TF>> optical_props_left;

    optical_props_subset = std::make_unique<Optical_props_1scl<TF>>(n_col_block, n_lay, *kdist_lw);

    std::unique_ptr<Source_func_lw<TF>> sources_subset;
    std::unique_ptr<Source_func_lw<TF>> sources_left;

    sources_subset = std::make_unique<Source_func_lw<TF>>(n_col_block, n_lay, *kdist_lw);

    if (n_col_block_left > 0)
    {
        optical_props_left = std::make_unique<Optical_props_1scl<TF>>(n_col_block_left, n_lay, *kdist_lw);
        sources_left = std::make_unique<Source_func_lw<TF>>(n_col_block_left, n_lay, *kdist_lw);
    }

    // Lambda function for solving optical properties subset.
    auto call_kernels = [&](
            const int col_s_in, const int col_e_in,
            std::unique_ptr<Optical_props_arry<double>>& optical_props_subset_in,
            Source_func_lw<double>& sources_subset_in,
            const Array<double,2>& emis_sfc_subset_in,
            Fluxes_broadband<double>& fluxes,
            Fluxes_broadband<double>& bnd_fluxes)
    {
        const int n_col_in = col_e_in - col_s_in + 1;
        Gas_concs<double> gas_concs_subset(gas_concs, col_s_in, n_col_in);

        kdist_lw->gas_optics(
                p_lay.subset({{ {col_s_in, col_e_in}, {1, n_lay} }}),
                p_lev.subset({{ {col_s_in, col_e_in}, {1, n_lev} }}),
                t_lay.subset({{ {col_s_in, col_e_in}, {1, n_lay} }}),
                t_sfc.subset({{ {col_s_in, col_e_in} }}),
                gas_concs_subset,
                optical_props_subset_in,
                sources_subset_in,
                col_dry.subset({{ {col_s_in, col_e_in}, {1, n_lay} }}),
                t_lev  .subset({{ {col_s_in, col_e_in}, {1, n_lev} }}) );

        Array<double,3> gpt_flux_up({n_col_in, n_lev, n_gpt});
        Array<double,3> gpt_flux_dn({n_col_in, n_lev, n_gpt});

        constexpr int n_ang = 1;

        Rte_lw<double>::rte_lw(
                optical_props_subset_in,
                top_at_1,
                sources_subset_in,
                emis_sfc_subset_in,
                Array<TF,2>(), // Add an empty array, no inc_flux.
                gpt_flux_up, gpt_flux_dn,
                n_ang);

        fluxes.reduce(gpt_flux_up, gpt_flux_dn, optical_props_subset_in, top_at_1);
        bnd_fluxes.reduce(gpt_flux_up, gpt_flux_dn, optical_props_subset_in, top_at_1);

        // Copy the data to the output.
        for (int ilev=1; ilev<=n_lev; ++ilev)
            for (int icol=1; icol<=n_col_in; ++icol)
            {
                lw_flux_up ({icol+col_s_in-1, ilev}) = fluxes.get_flux_up ()({icol, ilev});
                lw_flux_dn ({icol+col_s_in-1, ilev}) = fluxes.get_flux_dn ()({icol, ilev});
                lw_flux_net({icol+col_s_in-1, ilev}) = fluxes.get_flux_net()({icol, ilev});
            }

        for (int ibnd=1; ibnd<=n_bnd; ++ibnd)
            for (int ilev=1; ilev<=n_lev; ++ilev)
                for (int icol=1; icol<=n_col_in; ++icol)
                {
                    lw_bnd_flux_up ({icol+col_s_in-1, ilev, ibnd}) = bnd_fluxes.get_bnd_flux_up ()({icol, ilev, ibnd});
                    lw_bnd_flux_dn ({icol+col_s_in-1, ilev, ibnd}) = bnd_fluxes.get_bnd_flux_dn ()({icol, ilev, ibnd});
                    lw_bnd_flux_net({icol+col_s_in-1, ilev, ibnd}) = bnd_fluxes.get_bnd_flux_net()({icol, ilev, ibnd});
                }
    };

    for (int b=1; b<=n_blocks; ++b)
    {
        const int col_s = (b-1) * n_col_block + 1;
        const int col_e =  b    * n_col_block;

        Array<double,2> emis_sfc_subset = emis_sfc.subset({{ {1, n_bnd}, {col_s, col_e} }});

        std::unique_ptr<Fluxes_broadband<double>> fluxes_subset =
                std::make_unique<Fluxes_broadband<double>>(n_col_block, n_lev);
        std::unique_ptr<Fluxes_broadband<double>> bnd_fluxes_subset =
                std::make_unique<Fluxes_byband<double>>(n_col_block, n_lev, n_bnd);

        call_kernels(
                col_s, col_e,
                optical_props_subset,
                *sources_subset,
                emis_sfc_subset,
                *fluxes_subset,
                *bnd_fluxes_subset);
    }

    if (n_col_block_left > 0)
    {
        const int col_s = n_col - n_col_block_left + 1;
        const int col_e = n_col;

        Array<double,2> emis_sfc_left = emis_sfc.subset({{ {1, n_bnd}, {col_s, col_e} }});
        std::unique_ptr<Fluxes_broadband<double>> fluxes_left =
                std::make_unique<Fluxes_broadband<double>>(n_col_block_left, n_lev);
        std::unique_ptr<Fluxes_broadband<double>> bnd_fluxes_left =
                std::make_unique<Fluxes_byband<double>>(n_col_block_left, n_lev, n_bnd);

        call_kernels(
                col_s, col_e,
                optical_props_left,
                *sources_left,
                emis_sfc_left,
                *fluxes_left,
                *bnd_fluxes_left);
    }


    ////// SAVING THE MODEL OUTPUT //////
    master.print_message("Saving the output to NetCDF.\n");

    // Save the output of the optical solver to disk.
    Netcdf_file output_nc(master, "rte_rrtmgp_output.nc", Netcdf_mode::Create);
    output_nc.add_dimension("col", n_col);
    output_nc.add_dimension("lay", n_lay);
    output_nc.add_dimension("lev", n_lev);
    output_nc.add_dimension("gpt", n_gpt);
    output_nc.add_dimension("band", n_bnd);
    output_nc.add_dimension("pair", 2);

    auto nc_lay = output_nc.add_variable<TF>("lay", {"lay"});
    auto nc_lev = output_nc.add_variable<TF>("lev", {"lev"});

    nc_lay.insert(p_lay.v(), {0});
    nc_lev.insert(p_lev.v(), {0});
    
    // WARNING: The storage in the NetCDF interface uses C-ordering and indexing.
    // First, store the optical properties.
    auto nc_band_lims_wvn = output_nc.add_variable<TF>("band_lims_wvn", {"band", "pair"});
    auto nc_band_lims_gpt = output_nc.add_variable<int>("band_lims_gpt", {"band", "pair"});

    // nc_band_lims_wvn.insert(optical_props->get_band_lims_wavenumber().v(), {0, 0});
    // nc_band_lims_gpt.insert(optical_props->get_band_lims_gpoint().v()    , {0, 0});

    auto nc_tau = output_nc.add_variable<TF>("tau", {"gpt", "lay", "col"});
    // nc_tau.insert(optical_props->get_tau().v(), {0, 0, 0});

    // Second, store the sources.
    auto nc_lay_src     = output_nc.add_variable<TF>("lay_src"    , {"gpt", "lay", "col"});
    auto nc_lev_src_inc = output_nc.add_variable<TF>("lev_src_inc", {"gpt", "lay", "col"});
    auto nc_lev_src_dec = output_nc.add_variable<TF>("lev_src_dec", {"gpt", "lay", "col"});

    auto nc_sfc_src = output_nc.add_variable<TF>("sfc_src", {"gpt", "col"});

    // nc_lay_src.insert    (sources->get_lay_source().v()    , {0, 0, 0});
    // nc_lev_src_inc.insert(sources->get_lev_source_inc().v(), {0, 0, 0});
    // nc_lev_src_dec.insert(sources->get_lev_source_dec().v(), {0, 0, 0});

    // nc_sfc_src.insert(sources->get_sfc_source().v(), {0, 0});

    // Save the output of the flux calculation to disk.
    auto nc_flux_up  = output_nc.add_variable<TF>("lw_flux_up" , {"lev", "col"});
    auto nc_flux_dn  = output_nc.add_variable<TF>("lw_flux_dn" , {"lev", "col"});
    auto nc_flux_net = output_nc.add_variable<TF>("lw_flux_net", {"lev", "col"});

    auto nc_bnd_flux_up  = output_nc.add_variable<TF>("lw_bnd_flux_up" , {"band", "lev", "col"});
    auto nc_bnd_flux_dn  = output_nc.add_variable<TF>("lw_bnd_flux_dn" , {"band", "lev", "col"});
    auto nc_bnd_flux_net = output_nc.add_variable<TF>("lw_bnd_flux_net", {"band", "lev", "col"});

    nc_flux_up .insert(lw_flux_up .v(), {0, 0});
    nc_flux_dn .insert(lw_flux_dn .v(), {0, 0});
    nc_flux_net.insert(lw_flux_net.v(), {0, 0});

    nc_bnd_flux_up .insert(lw_bnd_flux_up .v(), {0, 0, 0});
    nc_bnd_flux_dn .insert(lw_bnd_flux_dn .v(), {0, 0, 0});
    nc_bnd_flux_net.insert(lw_bnd_flux_net.v(), {0, 0, 0});
}

int main()
{
    Master master;
    try
    {
        master.start();
        master.init();

        solve_radiation<FLOAT_TYPE>(master);
    }

    // Catch any exceptions and return 1.
    catch (const std::exception& e)
    {
        master.print_message("EXCEPTION: %s\n", e.what());
        return 1;
    }
    catch (...)
    {
        master.print_message("UNHANDLED EXCEPTION!\n");
        return 1;
    }

    // Return 0 in case of normal exit.
    return 0;
}
