        !COMPILER-GENERATED INTERFACE MODULE: Fri Oct 21 10:10:26 2011
        MODULE FORTRANCRS__genmod
          INTERFACE 
            SUBROUTINE FORTRANCRS(NNP,ANZNNEL,RESVEC,LOCVEC,CMATRX_CRS, &
     &INDEX_CRS,ROWOFFSET)
              INTEGER(KIND=4) :: ANZNNEL
              INTEGER(KIND=4) :: NNP
              REAL(KIND=8) :: RESVEC(NNP)
              REAL(KIND=8) :: LOCVEC(NNP)
              REAL(KIND=8) :: CMATRX_CRS(ANZNNEL)
              INTEGER(KIND=4) :: INDEX_CRS(ANZNNEL)
              INTEGER(KIND=4) :: ROWOFFSET(NNP+1)
            END SUBROUTINE FORTRANCRS
          END INTERFACE 
        END MODULE FORTRANCRS__genmod
