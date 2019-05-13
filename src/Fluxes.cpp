#include "Fluxes.h"
#include "Array.h"
#include "Optical_props.h"

namespace rrtmgp_kernels
{
    extern "C" void sum_broadband(
            int* ncol, int* nlev, int* ngpt,
            double* spectral_flux, double* broadband_flux);

    extern "C" void net_broadband_precalc(
            int* ncol, int* nlev,
            double* broadband_flux_dn, double* broadband_flux_up,
            double* broadband_flux_net);

    extern "C" void sum_byband(
            int* ncol, int* nlev, int* ngpt, int* nbnd,
            int* band_lims,
            double* spectral_flux,
            double* byband_flux);

    extern "C" void net_byband_precalc(
            int* ncol, int* nlev, int* nbnd,
            double* byband_flux_dn, double* byband_flux_up,
            double* byband_flux_net);

    template<typename TF>
    void sum_broadband(
            int ncol, int nlev, int ngpt,
            const Array<TF,3>& spectral_flux, Array<TF,2>& broadband_flux)
    {
        sum_broadband(
                &ncol, &nlev, &ngpt,
                const_cast<TF*>(spectral_flux.ptr()),
                broadband_flux.ptr());
    }

    template<typename TF>
    void net_broadband(
            int ncol, int nlev,
            const Array<TF,2>& broadband_flux_dn, const Array<TF,2>& broadband_flux_up,
            Array<TF,2>& broadband_flux_net)
    {
        net_broadband_precalc(
                &ncol, &nlev,
                const_cast<TF*>(broadband_flux_dn.ptr()),
                const_cast<TF*>(broadband_flux_up.ptr()),
                broadband_flux_net.ptr());
    }

    template<typename TF>
    void sum_byband(
            int ncol, int nlev, int ngpt, int nbnd,
            const Array<int,2>& band_lims,
            const Array<TF,3>& spectral_flux,
            Array<TF,3>& byband_flux)
    {
        sum_byband(
                &ncol, &nlev, &ngpt, &nbnd,
                const_cast<int*>(band_lims.ptr()),
                const_cast<TF*>(spectral_flux.ptr()),
                byband_flux.ptr());
    }

    template<typename TF>
    void net_byband(
            int ncol, int nlev, int nband,
            const Array<TF,3>& byband_flux_dn, const Array<TF,3>& byband_flux_up,
            Array<TF,3>& byband_flux_net)
    {
        net_byband_precalc(
                &ncol, &nlev, &nband,
                const_cast<TF*>(byband_flux_dn.ptr()),
                const_cast<TF*>(byband_flux_up.ptr()),
                byband_flux_net.ptr());
    }
}

template<typename TF>
Fluxes_broadband<TF>::Fluxes_broadband(const int ncol, const int nlev) :
    flux_up    ({ncol, nlev}),
    flux_dn    ({ncol, nlev}),
    flux_dn_dir({ncol, nlev}),
    flux_net   ({ncol, nlev})
{}

template<typename TF>
void Fluxes_broadband<TF>::reduce(
    const Array<TF,3>& gpt_flux_up, const Array<TF,3>& gpt_flux_dn,
    const std::unique_ptr<Optical_props_arry<TF>>& spectral_disc,
    const int top_at_1)
{
    const int ncol = gpt_flux_up.dim(1);
    const int nlev = gpt_flux_up.dim(2);
    const int ngpt = gpt_flux_up.dim(3);

    rrtmgp_kernels::sum_broadband(
            ncol, nlev, ngpt, gpt_flux_up, this->flux_up);

    rrtmgp_kernels::sum_broadband(
            ncol, nlev, ngpt, gpt_flux_dn, this->flux_dn);

    rrtmgp_kernels::net_broadband(
            ncol, nlev, this->flux_dn, this->flux_up, this->flux_net);
}

// CvH: unnecessary code duplication.
template<typename TF>
void Fluxes_broadband<TF>::reduce(
    const Array<TF,3>& gpt_flux_up, const Array<TF,3>& gpt_flux_dn, const Array<TF,3>& gpt_flux_dn_dir,
    const std::unique_ptr<Optical_props_arry<TF>>& spectral_disc,
    const int top_at_1)
{
    const int ncol = gpt_flux_up.dim(1);
    const int nlev = gpt_flux_up.dim(2);
    const int ngpt = gpt_flux_up.dim(3);

    reduce(gpt_flux_up, gpt_flux_dn, spectral_disc, top_at_1);

    rrtmgp_kernels::sum_broadband(
            ncol, nlev, ngpt,
            gpt_flux_dn_dir, this->flux_dn_dir);
}

template<typename TF>
Fluxes_byband<TF>::Fluxes_byband(const int ncol, const int nlev, const int nbnd) :
    Fluxes_broadband<TF>(ncol, nlev),
    bnd_flux_up    ({ncol, nlev, nbnd}),
    bnd_flux_dn    ({ncol, nlev, nbnd}),
    bnd_flux_dn_dir({ncol, nlev, nbnd}),
    bnd_flux_net   ({ncol, nlev, nbnd})
{}

template<typename TF>
void Fluxes_byband<TF>::reduce(
    const Array<TF,3>& gpt_flux_up,
    const Array<TF,3>& gpt_flux_dn,
    const std::unique_ptr<Optical_props_arry<TF>>& spectral_disc,
    const int top_at_1)
{
    const int ncol = gpt_flux_up.dim(1);
    const int nlev = gpt_flux_up.dim(2);
    const int ngpt = spectral_disc->get_ngpt();
    const int nbnd = spectral_disc->get_nband();

    const Array<int,2>& band_lims = spectral_disc->get_band_lims_gpoint();

    Fluxes_broadband<TF>::reduce(
            gpt_flux_up, gpt_flux_dn,
            spectral_disc, top_at_1);

    rrtmgp_kernels::sum_byband(
            ncol, nlev, ngpt, nbnd, band_lims,
            gpt_flux_up, this->bnd_flux_up);

    rrtmgp_kernels::sum_byband(
            ncol, nlev, ngpt, nbnd, band_lims,
            gpt_flux_dn, this->bnd_flux_dn);

    rrtmgp_kernels::net_byband(
            ncol, nlev, nbnd,
            this->bnd_flux_dn, this->bnd_flux_up, this->bnd_flux_net);
}

// CvH: a lot of code duplication.
template<typename TF>
void Fluxes_byband<TF>::reduce(
    const Array<TF,3>& gpt_flux_up,
    const Array<TF,3>& gpt_flux_dn,
    const Array<TF,3>& gpt_flux_dn_dir,
    const std::unique_ptr<Optical_props_arry<TF>>& spectral_disc,
    const int top_at_1)
{
    const int ncol = gpt_flux_up.dim(1);
    const int nlev = gpt_flux_up.dim(2);
    const int ngpt = spectral_disc->get_ngpt();
    const int nbnd = spectral_disc->get_nband();

    const Array<int,2>& band_lims = spectral_disc->get_band_lims_gpoint();

    Fluxes_broadband<TF>::reduce(
            gpt_flux_up, gpt_flux_dn, gpt_flux_dn_dir,
            spectral_disc, top_at_1);

    reduce(gpt_flux_up, gpt_flux_dn, spectral_disc, top_at_1);

    rrtmgp_kernels::sum_byband(
            ncol, nlev, ngpt, nbnd, band_lims,
            gpt_flux_dn_dir, this->bnd_flux_dn_dir);
}

template class Fluxes_broadband<double>;
template class Fluxes_byband<double>;