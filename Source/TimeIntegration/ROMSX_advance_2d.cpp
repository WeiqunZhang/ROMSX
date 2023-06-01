#include <ROMSX.H>
#include <Utils.H>

using namespace amrex;
//
// Start 2d step
//

void
ROMSX::advance_2d (int lev,
                   MultiFab& mf_u, MultiFab& mf_v,
                   MultiFab& mf_rhoS, MultiFab& mf_rhoA,
                   std::unique_ptr<MultiFab>& mf_ru,
                   std::unique_ptr<MultiFab>& mf_rv,
                   std::unique_ptr<MultiFab>& mf_rufrc,
                   std::unique_ptr<MultiFab>& mf_rvfrc,
                   std::unique_ptr<MultiFab>& mf_Zt_avg1,
                   std::unique_ptr<MultiFab>& mf_DU_avg1,
                   std::unique_ptr<MultiFab>& mf_DU_avg2,
                   std::unique_ptr<MultiFab>& mf_DV_avg1,
                   std::unique_ptr<MultiFab>& mf_DV_avg2,
                   std::unique_ptr<MultiFab>& mf_rubar,
                   std::unique_ptr<MultiFab>& mf_rvbar,
                   std::unique_ptr<MultiFab>& mf_rzeta,
                   std::unique_ptr<MultiFab>& mf_ubar,
                   std::unique_ptr<MultiFab>& mf_vbar,
                   std::unique_ptr<MultiFab>& mf_zeta,
                   std::unique_ptr<MultiFab>& mf_h,
                   std::unique_ptr<MultiFab>& mf_visc2_p,
                   std::unique_ptr<MultiFab>& mf_visc2_r,
                   const int ncomp, Real dt_lev, Real dtfast_lev,
                   bool predictor_2d_step,
                   bool first_2d_step, int my_iif, int nfast, int & next_indx1)
{
    auto geomdata  = Geom(lev).data();
    const auto dxi = Geom(lev).InvCellSizeArray();

    const int Lm = Geom(lev).Domain().size()[0];
    const int Mm = Geom(lev).Domain().size()[1];

    auto N = Geom(lev).Domain().size()[2]-1; // Number of vertical "levs" aka, NZ

    int iic = istep[lev];
    const int nrhs  = ncomp-1;
    const int nnew  = ncomp-1;
    const int nstp  = ncomp-1;
    int ntfirst = 0;
    //bool predictor_2d_step = true;
    //    int my_iif = 1; //substep index
    int knew = 3;
    int krhs = (my_iif + iic) % 2 + 1;
    int kstp = my_iif <=1 ? iic % 2 + 1 : (iic % 2 + my_iif % 2 + 1) % 2 + 1;
    int indx1 = krhs;
    if(predictor_2d_step)
        next_indx1 = 3 - indx1;
    else {
        knew = next_indx1;
        kstp = 3 - knew;
        krhs = 3;
        //If it's not the auxiliary time step, set indx1 to next_indx1
        if (my_iif<nfast+1)
            indx1=next_indx1;
    }
    int ptsk = 3-kstp;
    knew-=1;
    krhs-=1;
    kstp-=1;
    indx1-=1;
    ptsk-=1;
    Print() << "indices " <<knew<<"\t"<<krhs<<"\t"<<kstp<<"\t"<<predictor_2d_step<<"\t"<<(my_iif<=1)<<std::endl;


    for ( MFIter mfi(mf_u, TilingIfNotGPU()); mfi.isValid(); ++mfi )
    {
        Array4<Real> const& u = (mf_u).array(mfi);
        Array4<Real> const& v = (mf_v).array(mfi);
        Array4<Real> const& rhoS = (mf_rhoS).array(mfi);
        Array4<Real> const& rhoA = (mf_rhoA).array(mfi);
        Array4<Real> const& ubar = (mf_ubar)->array(mfi);
        Array4<Real> const& vbar = (mf_vbar)->array(mfi);
        Array4<Real> const& zeta = (mf_zeta)->array(mfi);
        Array4<Real> const& h = (mf_h)->array(mfi);
        Array4<Real> const& Zt_avg1 = (mf_Zt_avg1)->array(mfi);
        Array4<Real> const& DU_avg1 = (mf_DU_avg1)->array(mfi);
        Array4<Real> const& DU_avg2 = (mf_DU_avg2)->array(mfi);
        Array4<Real> const& DV_avg1 = (mf_DV_avg1)->array(mfi);
        Array4<Real> const& DV_avg2 = (mf_DV_avg2)->array(mfi);
        Array4<Real> const& ru = (mf_ru)->array(mfi);
        Array4<Real> const& rv = (mf_rv)->array(mfi);
        Array4<Real> const& rufrc = (mf_rufrc)->array(mfi);
        Array4<Real> const& rvfrc = (mf_rvfrc)->array(mfi);
        Array4<Real> const& rubar = (mf_rubar)->array(mfi);
        Array4<Real> const& rvbar = (mf_rvbar)->array(mfi);
        Array4<Real> const& rzeta = (mf_rzeta)->array(mfi);
        Array4<Real> const& visc2_p = (mf_visc2_p)->array(mfi);
        Array4<Real> const& visc2_r = (mf_visc2_r)->array(mfi);

        Box bx = mfi.tilebox();
        //copy the tilebox
        Box gbx1 = bx;
        Box gbx11 = bx;
        Box gbx2 = bx;
        //make only gbx be grown to match multifabs
        gbx2.grow(IntVect(NGROW,NGROW,0));
        gbx1.grow(IntVect(NGROW-1,NGROW-1,0));
        gbx11.grow(IntVect(NGROW-1,NGROW-1,NGROW-1));
        Box bxD = bx;
        Box ubxD = surroundingNodes(bx,0);
        Box vbxD = surroundingNodes(bx,1);
        bxD.makeSlab(2,0);
        ubxD.makeSlab(2,0);
        vbxD.makeSlab(2,0);
        Box gbx1D = bxD;
        Box gbx2D = bxD;
        gbx1D.grow(IntVect(NGROW-1,NGROW-1,0));
        gbx2D.grow(IntVect(NGROW,NGROW,0));
        //AKA
        //ubxD.setRange(2,0);
        //vbxD.setRange(2,0);

        FArrayBox fab_pn(gbx2,1,The_Async_Arena());
        FArrayBox fab_pm(gbx2,1,The_Async_Arena());
        FArrayBox fab_on_u(gbx2,1,The_Async_Arena());
        FArrayBox fab_om_v(gbx2,1,The_Async_Arena());
        FArrayBox fab_fomn(gbx2,1,The_Async_Arena());
        FArrayBox fab_Huon(gbx2,1,The_Async_Arena()); //fab_Huon.setVal(0.0);
        FArrayBox fab_Hvom(gbx2,1,The_Async_Arena()); //fab_Hvom.setVal(0.0);
        FArrayBox fab_oHz(gbx11,1,The_Async_Arena()); //fab_oHz.setVal(0.0);
        FArrayBox fab_om_u(gbx2,1,amrex::The_Async_Arena());
        FArrayBox fab_on_v(gbx2,1,amrex::The_Async_Arena());
        FArrayBox fab_om_r(gbx2,1,amrex::The_Async_Arena());
        FArrayBox fab_on_r(gbx2,1,amrex::The_Async_Arena());
        FArrayBox fab_om_p(gbx2,1,amrex::The_Async_Arena());
        FArrayBox fab_on_p(gbx2,1,amrex::The_Async_Arena());

        //step2d work arrays
        FArrayBox fab_Drhs(gbx2,1,The_Async_Arena());
        FArrayBox fab_Drhs_p(gbx2,1,The_Async_Arena());
        FArrayBox fab_Dnew(gbx2,1,The_Async_Arena());
        FArrayBox fab_zwrk(gbx2,1,The_Async_Arena());
        FArrayBox fab_gzeta(gbx2,1,The_Async_Arena());
        FArrayBox fab_gzeta2(gbx2,1,The_Async_Arena());
        FArrayBox fab_gzetaSA(gbx2,1,The_Async_Arena());
        FArrayBox fab_Dstp(gbx2,1,The_Async_Arena());
        FArrayBox fab_DUon(gbx2,1,The_Async_Arena());
        FArrayBox fab_DVom(gbx2,1,The_Async_Arena());
        FArrayBox fab_rhs_ubar(gbx2,1,The_Async_Arena());
        FArrayBox fab_rhs_vbar(gbx2,1,The_Async_Arena());
        FArrayBox fab_rhs_zeta(gbx2,1,The_Async_Arena());
        FArrayBox fab_zeta_new(gbx2,1,The_Async_Arena());

        auto on_u=fab_on_u.array();
        auto om_v=fab_om_v.array();
        auto pn=fab_pn.array();
        auto pm=fab_pm.array();
        auto fomn=fab_fomn.array();
        auto om_u=fab_om_u.array();
        auto on_v=fab_on_v.array();
        auto om_r=fab_om_r.array();
        auto on_r=fab_on_r.array();
        auto om_p=fab_om_p.array();
        auto on_p=fab_on_p.array();

        auto Drhs=fab_Drhs.array();
        auto Drhs_p=fab_Drhs_p.array();
        auto Dnew=fab_Dnew.array();
        auto zwrk=fab_zwrk.array();
        auto gzeta=fab_gzeta.array();
        auto gzeta2=fab_gzeta2.array();
        auto gzetaSA=fab_gzetaSA.array();
        auto Dstp=fab_Dstp.array();
        auto DUon=fab_DUon.array();
        auto DVom=fab_DVom.array();
        auto rhs_ubar=fab_rhs_ubar.array();
        auto rhs_vbar=fab_rhs_vbar.array();
        auto rhs_zeta=fab_rhs_zeta.array();
        auto zeta_new=fab_zeta_new.array();

        auto weight1 = vec_weight1.dataPtr();
        auto weight2 = vec_weight2.dataPtr();
        //Print() << "(0,0,0 start of advance2d" <<std::endl;
        //Print() << "(0,0,0 my_iif: " << my_iif <<std::endl;
        //Print() << "(0,0,0 iic: " << iic << std::endl;
        //Print().SetPrecision(18) << FArrayBox(ubar) << std::endl;
        //Print().SetPrecision(18) << FArrayBox(vbar) << std::endl;
        //Print().SetPrecision(18) << FArrayBox(rubar) << std::endl;
        //Print().SetPrecision(18) << FArrayBox(rvbar) << std::endl;

        //From ana_grid.h and metrics.F
        amrex::ParallelFor(gbx2,
        [=] AMREX_GPU_DEVICE (int i, int j, int)
        {
              pm(i,j,0)=dxi[0];
              pn(i,j,0)=dxi[1];
              DUon(i,j,0)=0.0;
              DVom(i,j,0)=0.0;
              rhs_ubar(i,j,0)=0.0;
              rhs_vbar(i,j,0)=0.0;
        });

        amrex::LoopConcurrentOnCpu(gbx2,
        [=] (int i, int j, int k)
        {
              //Note: are the comment definitons right? Don't seem to match metrics.f90
              om_v(i,j,0)=1.0/dxi[0]; // 2/(pm(i,j-1)+pm(i,j))
              on_u(i,j,0)=1.0/dxi[1]; // 2/(pm(i,j-1)+pm(i,j))
              om_r(i,j,0)=1.0/dxi[0]; // 1/pm(i,j)
              on_r(i,j,0)=1.0/dxi[1]; // 1/pn(i,j)
              //todo: om_p on_p
              om_p(i,j,0)=1.0/dxi[0]; // 4/(pm(i-1,j-1)+pm(i-1,j)+pm(i,j-1)+pm(i,j))
              on_p(i,j,0)=1.0/dxi[1]; // 4/(pn(i-1,j-1)+pn(i-1,j)+pn(i,j-1)+pn(i,j))
              on_v(i,j,0)=1.0/dxi[1]; // 2/(pn(i-1,j)+pn(i,j))
              om_u(i,j,0)=1.0/dxi[0]; // 2/(pm(i-1,j)+pm(i,j))
        });
        amrex::ParallelFor(gbx2,
        [=] AMREX_GPU_DEVICE (int i, int j, int  )
        {

              const auto prob_lo         = geomdata.ProbLo();
              const auto dx              = geomdata.CellSize();

              pm(i,j,0)=dxi[0];
              pn(i,j,0)=dxi[1];
              //defined UPWELLING
              Real f0=-8.26e-5;
              Real beta=0.0;
              Real Esize=1000*(Mm);
              Real y = prob_lo[1] + (j + 0.5) * dx[1];
              Real f=fomn(i,j,0)=f0+beta*(y-.5*Esize);
              fomn(i,j,0)=f*(1.0/(pm(i,j,0)*pn(i,j,0)));
        });

        amrex::ParallelFor(gbx2,
        [=] AMREX_GPU_DEVICE (int i, int j, int)
        {
            Drhs(i,j,0)=zeta(i,j,0,krhs)+h(i,j,0);
        });
        amrex::ParallelFor(gbx2D,
        [=] AMREX_GPU_DEVICE (int i, int j, int)
        {
            Real cff=.5*on_u(i,j,0);
            //todo: HACKHACKHACK may not work for evolve_free_surface=1 or flat_bathymetry=0
            Real cff1=i-1>=NGROW ? cff*(Drhs(i,j,0)+Drhs(i-1,j,0)) : on_u(i,j,0)*Drhs(i,j,0);
            DUon(i,j,0)=ubar(i,j,0,krhs)*cff1;
        });
        amrex::ParallelFor(gbx2D,
        [=] AMREX_GPU_DEVICE (int i, int j, int)
        {
            Real cff=.5*om_v(i,j,0);
            //todo: HACKHACKHACK may not work for evolve_free_surface=1 or flat_bathymetry=0
            Real cff1=j-1>=NGROW ? cff*(Drhs(i,j,0)+Drhs(i,j-1,0)) : om_v(i,j,0)*Drhs(i,j,0);
            DVom(i,j,0)=vbar(i,j,0,krhs)*cff1;
        });
        if(predictor_2d_step)
        {
            if(first_2d_step) {
                Real cff2=(Real(-1.0)/Real(12.0))*weight2[my_iif+1];
                amrex::ParallelFor(gbx2,
                [=] AMREX_GPU_DEVICE (int i, int j, int)
                {
                    Zt_avg1(i,j,0)=0.0;
                });
                amrex::ParallelFor(gbx2D,
                [=] AMREX_GPU_DEVICE (int i, int j, int)
                {
                    DU_avg1(i,j,0)=0.0;
                    DU_avg2(i,j,0)=cff2*DUon(i,j,0);
                });
                amrex::ParallelFor(gbx2D,
                [=] AMREX_GPU_DEVICE (int i, int j, int)
                {
                    DV_avg1(i,j,0)=0.0;
                    DV_avg2(i,j,0)=cff2*DVom(i,j,0);
                });
            }
            else {
                Real cff1=weight1[my_iif-1];
                Real cff2=(Real(8.0)/Real(12.0))*weight2[my_iif]-
                          (Real(1.0)/Real(12.0))*weight2[my_iif+1];
                amrex::ParallelFor(gbx2,
                [=] AMREX_GPU_DEVICE (int i, int j, int k)
                {
                    if(k==0)
                        Zt_avg1(i,j,0)=Zt_avg1(i,j,0)+cff1*zeta(i,j,0,krhs);
                });
                amrex::ParallelFor(gbx2D,
                [=] AMREX_GPU_DEVICE (int i, int j, int k)
                {
                    DU_avg1(i,j,0)=DU_avg1(i,j,0)+cff1*DUon(i,j,0);
                    DU_avg2(i,j,0)=DU_avg2(i,j,0)+cff2*DUon(i,j,0);
                });
                amrex::ParallelFor(gbx2D,
                [=] AMREX_GPU_DEVICE (int i, int j, int)
                {
                    DV_avg1(i,j,0)=DV_avg1(i,j,0)+cff1*DVom(i,j,0);
                    DV_avg2(i,j,0)=DV_avg2(i,j,0)+cff2*DVom(i,j,0);
                });
            }
        }
        else {
        Real cff2;
            if(first_2d_step)
                cff2=weight2[my_iif];
            else
                cff2=Real(5.0)/Real(12.0)*weight2[my_iif];
            amrex::ParallelFor(gbx2D,
            [=] AMREX_GPU_DEVICE (int i, int j, int)
            {
                DU_avg2(i,j,0)=DU_avg2(i,j,0)+cff2*DUon(i,j,0);
            });
            amrex::ParallelFor(gbx2D,
            [=] AMREX_GPU_DEVICE (int i, int j, int)
            {
                DV_avg2(i,j,0)=DV_avg2(i,j,0)+cff2*DVom(i,j,0);
            });
        }

        //
        //  Do not perform the actual time stepping during the auxiliary
        //  (nfast(ng)+1) time step. Jump to next box
        //

        if (my_iif>=nfast-1) {
            continue; }
        //Load new free-surface values into shared array at both predictor
        //and corrector steps
        if(solverChoice.evolve_free_surface) {
            //
            //=======================================================================
            //  Time step free-surface equation.
            //=======================================================================
            //
            //  During the first time-step, the predictor step is Forward-Euler
            //  and the corrector step is Backward-Euler. Otherwise, the predictor
            //  step is Leap-frog and the corrector step is Adams-Moulton.
            //

            // todo: gzeta

            // todo: HACKHACKHACK Should use rho0 from prob.H
            Real fac=1000.0/1025.0;

            if(my_iif==0) {
                Real cff1=dtfast_lev;
                amrex::ParallelFor(gbx1D,
                [=] AMREX_GPU_DEVICE (int i, int j, int )
                {
                    rhs_zeta(i,j,0)=(DUon(i,j,0)-DUon(i+1,j,0))+
                                    (DVom(i,j,0)-DVom(i,j+1,0));
                    zeta_new(i,j,0)=zeta(i,j,0,kstp)+
                                    pm(i,j,0)*pn(i,j,0)*cff1*rhs_zeta(i,j,0);
                    Dnew(i,j,0)=zeta_new(i,j,0)+h(i,j,0);

                    //Pressure gradient terms:
                    zwrk(i,j,0)=0.5_rt*(zeta(i,j,0,kstp)+zeta_new(i,j,0));
                    gzeta(i,j,0)=(fac+rhoS(i,j,0))*zwrk(i,j,0);
                    gzeta2(i,j,0)=gzeta(i,j,0)*zwrk(i,j,0);
                    gzetaSA(i,j,0)=zwrk(i,j,0)*(rhoS(i,j,0)-rhoA(i,j,0));
                });
            } else if (predictor_2d_step) {
                Real cff1=2.0_rt*dtfast_lev;
                Real cff4=4.0/25.0;
                Real cff5=1.0-2.0*cff4;
                amrex::ParallelFor(gbx1D,
                [=] AMREX_GPU_DEVICE (int i, int j, int )
                {
                    rhs_zeta(i,j,0)=(DUon(i,j,0)-DUon(i+1,j,0))+
                                    (DVom(i,j,0)-DVom(i,j+1,0));
                    zeta_new(i,j,0)=zeta(i,j,0,kstp)+
                                    pm(i,j,0)*pn(i,j,0)*cff1*rhs_zeta(i,j,0);
                    Dnew(i,j,0)=zeta_new(i,j,0)+h(i,j,0);
                    //Pressure gradient terms
                    zwrk(i,j,0)=cff5*zeta(i,j,0,krhs)+
                        cff4*(zeta(i,j,0,kstp)+zeta_new(i,j,0));
                    gzeta(i,j,0)=(fac+rhoS(i,j,0))*zwrk(i,j,0);
                    gzeta2(i,j,0)=gzeta(i,j,0)*zwrk(i,j,0);
                    gzetaSA(i,j,0)=zwrk(i,j,0)*(rhoS(i,j,0)-rhoA(i,j,0));
                });
            } else if (!predictor_2d_step) { //AKA if(corrector_2d_step)
                Real cff1=dtfast_lev*5.0_rt/12.0_rt;
                Real cff2=dtfast_lev*8.0_rt/12.0_rt;
                Real cff3=dtfast_lev*1.0_rt/12.0_rt;
                Real cff4=2.0_rt/5.0_rt;
                Real cff5=1.0_rt-cff4;
                amrex::ParallelFor(gbx1D,
                [=] AMREX_GPU_DEVICE (int i, int j, int )
                {
                    Real cff=cff1*((DUon(i,j,0)-DUon(i+1,j,0))+
                                   (DVom(i,j,0)-DVom(i,j+1,0)));
                    zeta_new(i,j,0)=zeta(i,j,0,kstp)+
                        pm(i,j,0)*pn(i,j,0)*(cff+
                                             cff2*rzeta(i,j,0,kstp)-
                                             cff3*rzeta(i,j,0,ptsk));
                    Dnew(i,j,0)=zeta_new(i,j,0)+h(i,j,0);
                    //Pressure gradient terms
                    zwrk(i,j,0)=cff5*zeta_new(i,j,0)+cff4*zeta(i,j,0,krhs);
                    gzeta(i,j,0)=(fac+rhoS(i,j,0))*zwrk(i,j,0);
                    gzeta2(i,j,0)=gzeta(i,j,0)*zwrk(i,j,0);
                    gzetaSA(i,j,0)=zwrk(i,j,0)*(rhoS(i,j,0)-rhoA(i,j,0));
                    });
            }

            //
            //  Load new free-surface values into shared array at both predictor
            //  and corrector steps.
            //
            amrex::ParallelFor(gbx1D,
            [=] AMREX_GPU_DEVICE (int i, int j, int )
            {
                zeta(i,j,0,knew)=zeta_new(i,j,0);
            });

            //
            //  If predictor step, load right-side-term into shared array.
            //
            if (predictor_2d_step) {
                amrex::ParallelFor(gbx1D,
                [=] AMREX_GPU_DEVICE (int i, int j, int )
                {
                    rzeta(i,j,0,krhs)=rhs_zeta(i,j,0);
                });
            }
        } else {
            amrex::ParallelFor(gbx2D,
            [=] AMREX_GPU_DEVICE (int i, int j, int )
            {
                zeta(i,j,0,knew)=zeta(i,j,0,kstp);
                Dnew(i,j,0)=zeta(i,j,0,knew)+h(i,j,0);
            });

            //
            //  If predictor step, load right-side-term into shared array.
            //
            if (predictor_2d_step) {
                amrex::ParallelFor(gbx2D,
                [=] AMREX_GPU_DEVICE (int i, int j, int )
                {
                    rzeta(i,j,0,krhs)=0.0;
                });
            }
        }

        //
        //=======================================================================
        //  Compute right-hand-side for the 2D momentum equations.
        //=======================================================================
        //
        /*
!
!-----------------------------------------------------------------------
!  Compute pressure gradient terms.
!-----------------------------------------------------------------------
!
      cff1=0.5_r8*g
      cff2=1.0_r8/3.0_r8
      DO j=Jstr,Jend
        DO i=IstrU,Iend
          rhs_ubar(i,j)=cff1*on_u(i,j)*                                 &
     &                  ((h(i-1,j)+                                     &
     &                    h(i ,j))*                                     &
     &                   (gzeta(i-1,j)-                                 &
     &                    gzeta(i  ,j))+                                &
     &                   (h(i-1,j)-                                     &
     &                    h(i  ,j))*                                    &
     &                   (gzetaSA(i-1,j)+                               &
     &                    gzetaSA(i  ,j)+                               &
     &                    cff2*(rhoA(i-1,j)-                            &
     &                          rhoA(i  ,j))*                           &
     &                         (zwrk(i-1,j)-                            &
     &                          zwrk(i  ,j)))+                          &
     &                   (gzeta2(i-1,j)-                                &
     &                    gzeta2(i  ,j)))
        END DO
        IF (j.ge.JstrV) THEN
          DO i=Istr,Iend
            rhs_vbar(i,j)=cff1*om_v(i,j)*                               &
     &                    ((h(i,j-1)+                                   &
     &                      h(i,j  ))*                                  &
     &                     (gzeta(i,j-1)-                               &
     &                      gzeta(i,j  ))+                              &
     &                     (h(i,j-1)-                                   &
     &                      h(i,j  ))*                                  &
     &                     (gzetaSA(i,j-1)+                             &
     &                      gzetaSA(i,j  )+                             &
     &                      cff2*(rhoA(i,j-1)-                          &
     &                            rhoA(i,j  ))*                         &
     &                           (zwrk(i,j-1)-                          &
     &                            zwrk(i,j  )))+                        &
     &                     (gzeta2(i,j-1)-                              &
     &                      gzeta2(i,j  )))
          END DO
        END IF
      END DO
*/
        // Advection terms for 2d ubar, vbar added to rhs_ubar and rhs_vbar
        //
        //-----------------------------------------------------------------------
        // rhs_2d
        //-----------------------------------------------------------------------
        //
        rhs_2d(bxD, ubar, vbar, rhs_ubar, rhs_vbar, DUon, DVom, krhs, N);

#ifdef UV_COR
        // Coriolis terms for 2d ubar, vbar added to rhs_ubar and rhs_vbar
        //
        //-----------------------------------------------------------------------
        // coriolis
        //-----------------------------------------------------------------------
        //
        // Need to clean up rhs_ubar vs rubar (index only the same for one out of predictor/corrector)
        coriolis(bxD, ubar, vbar, rhs_ubar, rhs_vbar, Drhs, fomn, krhs, 0);
#endif
        //Print() << "(10,10,0 mid of advance2d" <<std::endl;
        //Print().SetPrecision(18) << FArrayBox(ubar) << std::endl;
        //Print().SetPrecision(18) << FArrayBox(vbar) << std::endl;
        //Print().SetPrecision(18) << FArrayBox(rhs_ubar) << std::endl;
        //Print().SetPrecision(18) << FArrayBox(rhs_vbar) << std::endl;
        //Add in horizontal harmonic viscosity.
        // Consider generalizing or copying uv3dmix, where Drhs is used instead of Hz and u=>ubar v=>vbar, drop dt terms
        amrex::ParallelFor(gbx1,
        [=] AMREX_GPU_DEVICE (int i, int j, int)
        {
            Drhs_p(i,j,0)=0.25*(Drhs(i,j,0)+Drhs(i-1,j,0)+
                                Drhs(i,j-1,0)+Drhs(i-1,j-1,0));
        });

        uv3dmix(bxD, ubar, vbar, rhs_ubar, rhs_vbar, visc2_p, visc2_r, Drhs_p, on_r, om_r, on_p, om_p, pn, pm, krhs, nnew, 0.0);
        //Print() << "(0,0,0 after uv3dmix" << std::endl;
        //Print() << FArrayBox(rhs_ubar) << std::endl;
        //Print() << FArrayBox(rhs_vbar) << std::endl;
        //Print() << FArrayBox(rufrc) << std::endl;
        //Print() << FArrayBox(rvfrc) << std::endl;

        //Coupling from 3d to 2d
        /////////Coupling of 3d updates to 2d predictor-corrector
        //todo: my_iif=>my_my_iif iic => icc
        if (my_iif==0&&predictor_2d_step) {
            if (first_2d_step) {
                //Print() << "(0,0,0 before update" << std::endl;
                //Print() << FArrayBox(rhs_ubar) << std::endl;
                //Print() << FArrayBox(rhs_vbar) << std::endl;
                //Print() << FArrayBox(rufrc) << std::endl;
                //Print() << FArrayBox(rvfrc) << std::endl;
                amrex::ParallelFor(gbx1D,
                [=] AMREX_GPU_DEVICE (int i, int j, int )
                {
                    //DO j=Jstr,Jend
                    //DO i=IstrU,Iend
                    rufrc(i,j,0)=rufrc(i,j,0)-rhs_ubar(i,j,0);
                    rhs_ubar(i,j,0)=rhs_ubar(i,j,0)+rufrc(i,j,0);
                    ru(i,j,-1,nstp)=rufrc(i,j,0);
                });
                //END DO
                //END DO
                amrex::ParallelFor(gbx1D,
                [=] AMREX_GPU_DEVICE (int i, int j, int )
                {
                    //DO j=JstrV,Jend
                    //DO i=Istr,Iend
                    rvfrc(i,j,0)=rvfrc(i,j,0)-rhs_vbar(i,j,0);
                    rhs_vbar(i,j,0)=rhs_vbar(i,j,0)+rvfrc(i,j,0);
                    rv(i,j,-1,nstp)=rvfrc(i,j,0);
                });
                //Print() << "(0,0,0 after update" << std::endl;
                //Print() << FArrayBox(rhs_ubar) << std::endl;
                //Print() << FArrayBox(rhs_vbar) << std::endl;
                //Print() << FArrayBox(rufrc) << std::endl;
                //Print() << FArrayBox(rvfrc) << std::endl;
                //  END DO
                //  END DO
            } else if (iic==(ntfirst+1)) {
                amrex::ParallelFor(gbx1D,
                [=] AMREX_GPU_DEVICE (int i, int j, int )
                {
                    //DO j=Jstr,Jend
                    //DO i=IstrU,Iend
                      rufrc(i,j,0)=rufrc(i,j,0)-rhs_ubar(i,j,0);
                      rhs_ubar(i,j,0)=rhs_ubar(i,j,0)+
                          1.5_rt*rufrc(i,j,0)-0.5_rt*ru(i,j,-1,nnew);
                      ru(i,j,-1,nstp)=rufrc(i,j,0);
                });
                                   //END DO
                                   //          END DO
                amrex::ParallelFor(gbx1D,
                [=] AMREX_GPU_DEVICE (int i, int j, int )
                {
                    //DO j=JstrV,Jend
                    //DO i=Istr,Iend
                    rvfrc(i,j,0)=rvfrc(i,j,0)-rhs_vbar(i,j,0);
                    rhs_vbar(i,j,0)=rhs_vbar(i,j,0)+
                        1.5_rt*rvfrc(i,j,0)-0.5_rt*rv(i,j,-1,nnew);
                    rv(i,j,-1,nstp)=rvfrc(i,j,0);
                });
                                   //END DO
                                   //          END DO
            } else {
                Real cff1=23.0_rt/12.0_rt;
                Real cff2=16.0_rt/12.0_rt;
                Real cff3= 5.0_rt/12.0_rt;
                amrex::ParallelFor(gbx1D,
                [=] AMREX_GPU_DEVICE (int i, int j, int )
                {
                    //DO j=Jstr,Jend
                    //DO i=IstrU,Iend
                    rufrc(i,j,0)=rufrc(i,j,0)-rhs_ubar(i,j,0);
                    rhs_ubar(i,j,0)=rhs_ubar(i,j,0)+
                        cff1*rufrc(i,j,0)-
                        cff2*ru(i,j,-1,nnew)+
                        cff3*ru(i,j,-1,nstp);
                    ru(i,j,-1,nstp)=rufrc(i,j,0);
                });
                  //END DO
                  //END DO
                amrex::ParallelFor(gbx1D,
                [=] AMREX_GPU_DEVICE (int i, int j, int )
                {
                    //DO j=JstrV,Jend
                    //DO i=Istr,Iend
                    rvfrc(i,j,0)=rvfrc(i,j,0)-rhs_vbar(i,j,0);
                    rhs_vbar(i,j,0)=rhs_vbar(i,j,0)+
                          cff1*rvfrc(i,j,0)-
                          cff2*rv(i,j,-1,nnew)+
                          cff3*rv(i,j,-1,nstp);
                    rv(i,j,-1,nstp)=rvfrc(i,j,0);
                });
                //END DO
                //END DO
            }
        } else {
            //    Print() << "(0,0,0 before update 1 u" << std::endl;
            //    Print() << FArrayBox(rhs_ubar) << std::endl;
            //    Print() << FArrayBox(rufrc) << std::endl;
            amrex::ParallelFor(gbx1D,
            [=] AMREX_GPU_DEVICE (int i, int j, int )
            {
                //DO j=Jstr,Jend
                //DO i=IstrU,Iend
                rhs_ubar(i,j,0)=rhs_ubar(i,j,0)+rufrc(i,j,0);
            });
            //    Print() << "(0,0,0 after update 1 u" << std::endl;
            //    Print() << FArrayBox(rhs_ubar) << std::endl;
            //    Print() << FArrayBox(rufrc) << std::endl;
            //END DO
            //END DO
            //    Print() << "(0,0,0 before update 1 v" << std::endl;
            //    Print() << FArrayBox(rhs_vbar) << std::endl;
            //    Print() << FArrayBox(rvfrc) << std::endl;
            amrex::ParallelFor(gbx1D,
            [=] AMREX_GPU_DEVICE (int i, int j, int )
            {
                //DO j=JstrV,Jend
                //DO i=Istr,Iend
                rhs_vbar(i,j,0)=rhs_vbar(i,j,0)+rvfrc(i,j,0);
            });
            //    Print() << "(0,0,0 after update 1 v" << std::endl;
            //    Print() << FArrayBox(rhs_vbar) << std::endl;
            //    Print() << FArrayBox(rvfrc) << std::endl;
            //END DO
            //END DO
        }

        //Print().SetPrecision(18) << FArrayBox(rhs_vbar) << std::endl;

        //
        //=======================================================================
        //  Time step 2D momentum equations.
        //=======================================================================
        //
        //  Compute total water column depth.
        //
        amrex::ParallelFor(gbx2D,
        [=] AMREX_GPU_DEVICE (int i, int j, int )
        {
        //DO j=JstrV-1,Jend
            //DO i=IstrU-1,Iend
              Dstp(i,j,0)=zeta(i,j,0,kstp)+h(i,j,0);
        });
        //END DO
        //END DO

        //
        //  During the first time-step, the predictor step is Forward-Euler
        //  and the corrector step is Backward-Euler. Otherwise, the predictor
        //  step is Leap-frog and the corrector step is Adams-Moulton.
        //
        if (my_iif==0) {
            Real cff1=0.5_rt*dtfast_lev;
            amrex::ParallelFor(gbx1D,
            [=] AMREX_GPU_DEVICE (int i, int j, int )
            {
                //DO j=Jstr,Jend
                //DO i=IstrU,Iend
                Real cff=(pm(i,j,0)+pm(i-1,j,0))*(pn(i,j,0)+pn(i-1,j,0));
                Real fac=1.0_rt/(Dnew(i,j,0)+Dnew(i-1,j,0));
                ubar(i,j,0,knew)=(ubar(i,j,0,kstp)*
                                 (Dstp(i,j,0)+Dstp(i-1,j,0))+
                                  cff*cff1*rhs_ubar(i,j,0))*fac;
            });
            //END DO
            //END DO
            amrex::ParallelFor(gbx1D,
            [=] AMREX_GPU_DEVICE (int i, int j, int )
            {
                //DO j=JstrV,Jend
                //DO i=Istr,Iend
                Real cff=(pm(i,j,0)+pm(i,j-1,0))*(pn(i,j,0)+pn(i,j-1,0));
                Real fac=1.0_rt/(Dnew(i,j,0)+Dnew(i,j-1,0));
                vbar(i,j,0,knew)=(vbar(i,j,0,kstp)*
                                 (Dstp(i,j,0)+Dstp(i,j-1,0))+
                                  cff*cff1*rhs_vbar(i,j,0))*fac;
            });
            //END DO
            //END DO
        } else if (predictor_2d_step) {
            Real cff1=dtfast_lev;
            amrex::ParallelFor(gbx1D,
            [=] AMREX_GPU_DEVICE (int i, int j, int )
            {
                //DO j=Jstr,Jend
                //DO i=IstrU,Iend
                Real cff=(pm(i,j,0)+pm(i-1,j,0))*(pn(i,j,0)+pn(i-1,j,0));
                Real fac=1.0_rt/(Dnew(i,j,0)+Dnew(i-1,j,0));
                ubar(i,j,0,knew)=(ubar(i,j,0,kstp)*
                                 (Dstp(i,j,0)+Dstp(i-1,j,0))+
                                  cff*cff1*rhs_ubar(i,j,0))*fac;
            });
            //END DO
            //END DO
            amrex::ParallelFor(gbx1D,
            [=] AMREX_GPU_DEVICE (int i, int j, int )
            {
                //DO j=JstrV,Jend
                //DO i=Istr,Iend
                Real cff=(pm(i,j,0)+pm(i,j-1,0))*(pn(i,j,0)+pn(i,j-1,0));
                Real fac=1.0_rt/(Dnew(i,j,0)+Dnew(i,j-1,0));
                vbar(i,j,0,knew)=(vbar(i,j,0,kstp)*
                                 (Dstp(i,j,0)+Dstp(i,j-1,0))+
                                  cff*cff1*rhs_vbar(i,j,0))*fac;
            });
            //END DO
            //END DO
        } else if ((!predictor_2d_step)) {
            Real cff1=0.5_rt*dtfast_lev*5.0_rt/12.0_rt;
            Real cff2=0.5_rt*dtfast_lev*8.0_rt/12.0_rt;
            Real cff3=0.5_rt*dtfast_lev*1.0_rt/12.0_rt;
            amrex::ParallelFor(gbx1D,
            [=] AMREX_GPU_DEVICE (int i, int j, int )
            {
                //DO j=Jstr,Jend
                //DO i=IstrU,Iend
                Real cff=(pm(i,j,0)+pm(i-1,j,0))*(pn(i,j,0)+pn(i-1,j,0));
                Real fac=1.0_rt/(Dnew(i,j,0)+Dnew(i-1,j,0));
                ubar(i,j,0,knew)=(ubar(i,j,0,kstp)*
                                 (Dstp(i,j,0)+Dstp(i-1,j,0))+
                                 cff*(cff1*rhs_ubar(i,j,0)+
                                      cff2*rubar(i,j,0,kstp)-
                                      cff3*rubar(i,j,0,ptsk)))*fac;
            });
            //END DO
            //END DO
            amrex::ParallelFor(gbx1D,
            [=] AMREX_GPU_DEVICE (int i, int j, int )
            {
                //DO j=JstrV,Jend
                //DO i=Istr,Iend
                Real cff=(pm(i,j,0)+pm(i,j-1,0))*(pn(i,j,0)+pn(i,j-1,0));
                Real fac=1.0_rt/(Dnew(i,j,0)+Dnew(i,j-1,0));
                vbar(i,j,0,knew)=(vbar(i,j,0,kstp)*
                                 (Dstp(i,j,0)+Dstp(i,j-1,0))+
                                 cff*(cff1*rhs_vbar(i,j,0)+
                                      cff2*rvbar(i,j,0,kstp)-
                                      cff3*rvbar(i,j,0,ptsk)))*fac;
            });
            //END DO
            //END DO
        }

        //store rhs_ubar and rhs_vbar to save later
        //
        //  If predictor step, load right-side-term into shared arrays for
        //  future use during the subsequent corrector step.
        //

        if (predictor_2d_step) {
            amrex::ParallelFor(gbx1D,
            [=] AMREX_GPU_DEVICE (int i, int j, int )
            {
                //DO j=Jstr,Jend
                //DO i=IstrU,Iend
                rubar(i,j,0,krhs)=rhs_ubar(i,j,0);
            });
            //END DO
            //END DO
            amrex::ParallelFor(gbx1D,
            [=] AMREX_GPU_DEVICE (int i, int j, int )
            {
                //DO j=JstrV,Jend
                //DO i=Istr,Iend
                rvbar(i,j,0,krhs)=rhs_vbar(i,j,0);
            });
            //END DO
            //END DO
        }
        //Print() << "(0,0,0 end of advance2d" <<std::endl;
        //Print().SetPrecision(18) << FArrayBox(ubar) << std::endl;
        //Print().SetPrecision(18) << FArrayBox(vbar) << std::endl;
        //Print().SetPrecision(18) << FArrayBox(zeta) << std::endl;
        //Print().SetPrecision(18) << FArrayBox(rubar) << std::endl;
        //Print().SetPrecision(18) << FArrayBox(rvbar) << std::endl;
        //Print().SetPrecision(18) << FArrayBox(rzeta) << std::endl;
    }
    mf_DU_avg1->FillBoundary(geom[lev].periodicity());
    mf_DU_avg2->FillBoundary(geom[lev].periodicity());
    mf_DV_avg1->FillBoundary(geom[lev].periodicity());
    mf_DV_avg2->FillBoundary(geom[lev].periodicity());
    mf_rubar->FillBoundary(geom[lev].periodicity());
    mf_rvbar->FillBoundary(geom[lev].periodicity());
    mf_rzeta->FillBoundary(geom[lev].periodicity());
    mf_ubar->FillBoundary(geom[lev].periodicity());
    mf_vbar->FillBoundary(geom[lev].periodicity());
    mf_zeta->FillBoundary(geom[lev].periodicity());

}
