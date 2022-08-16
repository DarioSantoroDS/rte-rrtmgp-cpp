//
// Created by Mirjam Tijhuis on 05/08/2022.
//
#include <limits>
#include "Aerosol_optics.h"

Aerosol_optics::Aerosol_optics(
        const Array<Float,2>& band_lims_wvn, const std::vector<Float>& rh_upper,
        const Array<Float,2>& mext_phobic, const Array<Float,2>& ssa_phobic, const Array<Float,2>& g_phobic,
        const Array<Float,3>& mext_philic, const Array<Float,3>& ssa_philic, const Array<Float,3>& g_philic) :
        Optical_props(band_lims_wvn)
{
        // Load coefficients.
    this->mext_phobic = mext_phobic;
    this->ssa_phobic = ssa_phobic;
    this->g_phobic = g_phobic;

    this->mext_philic = mext_philic;
    this->ssa_philic = ssa_philic;
    this->g_philic = g_philic;

    this->rh_upper = rh_upper;
}


int rh_class(Float rel_hum, std::vector<Float> rh2)
{
    int ihum = 0;
    Float rh_class = rh2[ihum];
    while (rh_class < rel_hum)
    {
        ihum += 1;
        rh_class = rh2[ihum];
    }
    return ihum;
}

void compute_all_from_table(
        const int ncol, const int nlay, const int nbnd,
        const Array<Float,2>& aermr01, const Array<Float,2>& aermr02, const Array<Float,2>& aermr03,
        const Array<Float,2>& aermr04, const Array<Float,2>& aermr05, const Array<Float,2>& aermr06,
        const Array<Float,2>& aermr07, const Array<Float,2>& aermr08, const Array<Float,2>& aermr09,
        const Array<Float,2>& aermr10, const Array<Float,2>& aermr11,
        const Array<Float,2>& rh, const Array<Float,2>& dpg, const std::vector<Float>& rh_classes,
        const Array<Float, 2>& mext_phobic, const Array<Float, 2>& ssa_phobic, const Array<Float, 2>& g_phobic,
        const Array<Float, 3>& mext_philic, const Array<Float, 3>& ssa_philic, const Array<Float, 3>& g_philic,
        Array<Float,3>& tau, Array<Float,3>& taussa, Array<Float,3>& taussag)
{
    std::string list_aerosol_types[]{"SS1", "SS2", "SS3", "DU1", "DU2", "DU3", "OM1", "OM2", "BC1", "BC2", "SU"};
    for (int ibnd=1; ibnd<=nbnd; ++ibnd)
        for (int ilay=1; ilay<=nlay; ++ilay)
            for (int icol=1; icol<=ncol; ++icol)
            {
                Float tau_local = 0;
                Float taussa_local = 0;
                Float taussag_local = 0;
                Float mmr;
                Float mext;
                Float ssa;
                Float g;

                for (auto &&aerosol_class : list_aerosol_types)
                {
                    int ihum = rh_class(rh({icol, ilay}), rh_classes) + 1;

                    if (aerosol_class == "DU1")
                    {
                        mext = mext_phobic({ibnd, 1});
                        ssa = ssa_phobic({ibnd, 1});
                        g = g_phobic({ibnd, 1});
                        mmr = aermr04({icol, ilay});
                    }
                    else if (aerosol_class == "DU2")
                    {
                        mext = mext_phobic({ibnd, 8});
                        ssa = ssa_phobic({ibnd,8});
                        g = g_phobic({ibnd, 8});
                        mmr = aermr05({icol, ilay});
                    }
                    else if (aerosol_class == "DU3")
                    {
                        mext = mext_phobic({ibnd, 6});
                        ssa = ssa_phobic({ibnd, 6});
                        g = g_phobic({ibnd, 6});
                        mmr = aermr06({icol, ilay});
                    }
                    else if (aerosol_class == "BC1")
                    {
                        mext = mext_phobic({ibnd, 11});
                        ssa = ssa_phobic({ibnd, 11});
                        g = g_phobic({ibnd, 11});
                        mmr = aermr09({icol, ilay});
                    }
                    else if (aerosol_class == "BC2")
                    {
                        mext = mext_phobic({ibnd, 11});
                        ssa = ssa_phobic({ibnd, 11});
                        g = g_phobic({ibnd, 11});
                        mmr = aermr10({icol, ilay});
                    }
                    else if (aerosol_class == "SS1")
                    {
                        mext = mext_philic({ibnd, ihum, 1});
                        ssa = ssa_philic({ibnd, ihum, 1});
                        g = g_philic({ibnd, ihum, 1});
                        mmr = aermr01({icol, ilay});
                    }
                    else if (aerosol_class == "SS2")
                    {
                        mext = mext_philic({ibnd, ihum, 2});
                        ssa = ssa_philic({ibnd, ihum, 2});
                        g = g_philic({ibnd, ihum, 2});
                        mmr = aermr02({icol, ilay});
                    }
                    else if (aerosol_class == "SS3")
                    {
                        mext = mext_philic({ibnd, ihum, 3});
                        ssa = ssa_philic({ibnd, ihum, 3});
                        g = g_philic({ibnd, ihum, 3});
                        mmr = aermr03({icol, ilay});
                    }
                    else if (aerosol_class == "SU")
                    {
                        mext = mext_philic({ibnd, ihum, 5});
                        ssa = ssa_philic({ibnd, ihum, 5});
                        g = g_philic({ibnd, ihum, 5});
                        mmr = aermr11({icol, ilay});
                    }
                    else if (aerosol_class == "OM1")
                    {
                        mext = mext_phobic({ibnd, 10});
                        ssa = ssa_phobic({ibnd, 10});
                        g = g_phobic({ibnd, 10});
                        mmr = aermr08({icol, ilay});
                    }
                    else if (aerosol_class == "OM2")
                    {
                        mext = mext_philic({ibnd, ihum, 4});
                        ssa = ssa_philic({ibnd, ihum, 4});
                        g = g_philic({ibnd, ihum, 4});
                        mmr = aermr07({icol, ilay});
                    }

                    Float local_od = mmr * dpg({icol, ilay}) * mext;
                    tau_local += local_od;
                    taussa_local += local_od * ssa;
                    taussag_local += local_od * ssa * g;

                }

                tau    ({icol, ilay, ibnd}) = tau_local;
                taussa ({icol, ilay, ibnd}) = taussa_local;
                taussag({icol, ilay, ibnd}) = taussag_local;
            }
}

// Two-stream variant of aerosol optics.
void Aerosol_optics::aerosol_optics(const Array<Float, 2> &aermr01, const Array<Float, 2> &aermr02, const Array<Float, 2> &aermr03,
                                    const Array<Float, 2> &aermr04, const Array<Float, 2> &aermr05, const Array<Float, 2> &aermr06,
                                    const Array<Float, 2> &aermr07, const Array<Float, 2> &aermr08, const Array<Float, 2> &aermr09,
                                    const Array<Float, 2> &aermr10, const Array<Float, 2> &aermr11,
                                    const Array<Float, 2> &rh, const Array<Float, 2> &dpg,
                                    Optical_props_2str &optical_props)
{
    const int ncol = rh.dim(1);
    const int nlay = rh.dim(2);
    const int nbnd = this->get_nband();

    // Temporary arrays for storage.
    Array<Float,3> ltau    ({ncol, nlay, nbnd});
    Array<Float,3> ltaussa ({ncol, nlay, nbnd});
    Array<Float,3> ltaussag({ncol, nlay, nbnd});

    compute_all_from_table(
            ncol, nlay, nbnd, aermr01, aermr02, aermr03, aermr04, aermr05, aermr06, aermr07, aermr08, aermr09, aermr10, aermr11,
            rh, dpg,
            this->rh_upper,
            this->mext_phobic, this->ssa_phobic, this->g_phobic,
            this->mext_philic, this->ssa_philic, this->g_philic,
            ltau, ltaussa, ltaussag);

    constexpr Float eps = std::numeric_limits<Float>::epsilon();

    // Process the calculated optical properties.
    for (int ibnd=1; ibnd<=nbnd; ++ibnd)
        for (int ilay=1; ilay<=nlay; ++ilay)
                for (int icol=1; icol<=ncol; ++icol)
                {
                    const Float tau = ltau({icol, ilay, ibnd});
                    const Float taussa = ltaussa({icol, ilay, ibnd});
                    const Float taussag = ltaussag({icol, ilay, ibnd});

                    optical_props.get_tau()({icol, ilay, ibnd}) = tau;
                    optical_props.get_ssa()({icol, ilay, ibnd}) = taussa / std::max(tau, eps);
                    optical_props.get_g  ()({icol, ilay, ibnd}) = taussag / std::max(taussa, eps);
                }
}


