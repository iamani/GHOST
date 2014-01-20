#include "ghost/io.h"
#include "ghost/complex.h"

template<typename m_t, typename f_t> void ghost_castArray_tmpl(void *out, void *in, int nEnts)
{
    ghost_mnnz_t i;
    m_t *md = (m_t *)out;
    f_t *fd = (f_t *)in;

    for (i = 0; i<nEnts; i++) {
        md[i] = (m_t)(fd[i]);
    }
}

extern "C" void dd_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< double,double >(matrixData, fileData, nEnts); }

extern "C" void ds_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< double,float >(matrixData, fileData, nEnts); }

extern "C" void dc_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< double,ghost_complex<float> >(matrixData, fileData, nEnts); }

extern "C" void dz_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< double,ghost_complex<double> >(matrixData, fileData, nEnts); }

extern "C" void sd_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< float,double >(matrixData, fileData, nEnts); }

extern "C" void ss_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< float,float >(matrixData, fileData, nEnts); }

extern "C" void sc_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< float,ghost_complex<float> >(matrixData, fileData, nEnts); }

extern "C" void sz_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< float,ghost_complex<double> >(matrixData, fileData, nEnts); }

extern "C" void cd_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< ghost_complex<float>,double >(matrixData, fileData, nEnts); }

extern "C" void cs_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< ghost_complex<float>,float >(matrixData, fileData, nEnts); }

extern "C" void cc_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< ghost_complex<float>,ghost_complex<float> >(matrixData, fileData, nEnts); }

extern "C" void cz_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< ghost_complex<float>,ghost_complex<double> >(matrixData, fileData, nEnts); }

extern "C" void zd_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< ghost_complex<double>,double >(matrixData, fileData, nEnts); }

extern "C" void zs_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< ghost_complex<double>,float >(matrixData, fileData, nEnts); }

extern "C" void zc_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< ghost_complex<double>,ghost_complex<float> >(matrixData, fileData, nEnts); }

extern "C" void zz_ghost_castArray(void *matrixData, void *fileData, int nEnts)
{ return ghost_castArray_tmpl< ghost_complex<double>,ghost_complex<double> >(matrixData, fileData, nEnts); }
