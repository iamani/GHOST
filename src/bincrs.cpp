#include "ghost/bincrs.h"
#include <complex>

template<typename m_t, typename f_t> void ghost_castarray_tmpl(void *out, void *in, int nEnts)
{
    ghost_lidx i;
    m_t *md = (m_t *)out;
    f_t *fd = (f_t *)in;

    for (i = 0; i<nEnts; i++) {
        md[i] = (m_t)fd[i];
    }
}

extern "C" void dd_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< double,double >(matrixData, fileData, nEnts); }

extern "C" void ds_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< double,float >(matrixData, fileData, nEnts); }

extern "C" void dc_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< double,std::complex<float> >(matrixData, fileData, nEnts); }

extern "C" void dz_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< double,std::complex<double> >(matrixData, fileData, nEnts); }

extern "C" void sd_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< float,double >(matrixData, fileData, nEnts); }

extern "C" void ss_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< float,float >(matrixData, fileData, nEnts); }

extern "C" void sc_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< float,std::complex<float> >(matrixData, fileData, nEnts); }

extern "C" void sz_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< float,std::complex<double> >(matrixData, fileData, nEnts); }

extern "C" void cd_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< std::complex<float>,double >(matrixData, fileData, nEnts); }

extern "C" void cs_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< std::complex<float>,float >(matrixData, fileData, nEnts); }

extern "C" void cc_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< std::complex<float>,std::complex<float> >(matrixData, fileData, nEnts); }

extern "C" void cz_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< std::complex<float>,std::complex<double> >(matrixData, fileData, nEnts); }

extern "C" void zd_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< std::complex<double>,double >(matrixData, fileData, nEnts); }

extern "C" void zs_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< std::complex<double>,float >(matrixData, fileData, nEnts); }

extern "C" void zc_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< std::complex<double>,std::complex<float> >(matrixData, fileData, nEnts); }

extern "C" void zz_ghost_castarray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castarray_tmpl< std::complex<double>,std::complex<double> >(matrixData, fileData, nEnts); }

