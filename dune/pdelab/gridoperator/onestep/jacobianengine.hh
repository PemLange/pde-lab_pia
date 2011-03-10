#ifndef DUNE_ONE_STEP_JACOBIANENGINE_HH
#define DUNE_ONE_STEP_JACOBIANENGINE_HH

namespace Dune{
  namespace PDELab{

    /**
       \brief The local assembler engine for UDG sub triangulations which
       assembles the residual vector

       \tparam LA The local udg assembler

    */
    template<typename OSLA>
    class OneStepLocalJacobianAssemblerEngine
    {
    public:
      //! The type of the wrapping local assembler
      typedef OSLA LocalAssembler;

      typedef typename OSLA::LocalAssemblerDT0 LocalAssemblerDT0;
      typedef typename OSLA::LocalAssemblerDT1 LocalAssemblerDT1;

      typedef typename LocalAssemblerDT0::LocalJacobianAssemblerEngine JacobianEngineDT0;
      typedef typename LocalAssemblerDT1::LocalJacobianAssemblerEngine JacobianEngineDT1;

      //! The type of the residual vector
      typedef typename OSLA::Jacobian Jacobian;

      //! The type of the solution vector
      typedef typename OSLA::Solution Solution;

      //! The type for real numbers
      typedef typename OSLA::Real Real;

      /**
         \brief Constructor 

         \param [in] local_assembler_ The local assembler object which
         creates this engine
      */
      OneStepLocalJacobianAssemblerEngine(const LocalAssembler & local_assembler_)
        : la(local_assembler_), 
          invalid_lae0(static_cast<JacobianEngineDT0*>(0)), lae0(invalid_lae0), 
          invalid_lae1(static_cast<JacobianEngineDT1*>(0)), lae1(invalid_lae1), 
          invalid_jacobian(static_cast<Jacobian*>(0)), 
          invalid_solution(static_cast<Solution*>(0)),
          jacobian(invalid_jacobian), solution(invalid_solution)
      {}

      //! Query methods for the global grid assembler
      //! @{
      bool requireSkeleton() const 
      { return implicit && (lae0->requireSkeleton() || lae1->requireSkeleton()); }
      bool requireSkeletonTwoSided() const
      { return lae0->requireSkeletonTwoSided() || lae1->requireSkeletonTwoSided(); }
      bool requireUVVolume() const
      { return lae0->requireUVVolume() || lae1->requireUVVolume(); }
      bool requireVVolume() const
      { return lae0->requireVVolume() || lae1->requireVVolume(); }
      bool requireUVSkeleton() const
      { return lae0->requireUVSkeleton() || lae1->requireUVSkeleton(); }
      bool requireVSkeleton() const
      { return lae0->requireVSkeleton() || lae1->requireVSkeleton(); }
      bool requireUVBoundary() const
      { return lae0->requireUVBoundary() || lae1->requireUVBoundary(); }
      bool requireVBoundary() const
      { return lae0->requireVBoundary() || lae1->requireVBoundary(); }
      bool requireUVProcessor() const
      { return lae0->requireUVProcessor() || lae1->requireUVProcessor(); }
      bool requireVProcessor() const
      { return lae0->requireVProcessor() || lae1->requireVProcessor(); }
      bool requireUVEnrichedCoupling() const
      { return lae0->requireUVEnrichedCoupling() || lae1->requireUVEnrichedCoupling(); }
      bool requireVEnrichedCoupling() const
      { return lae0->requireVEnrichedCoupling() || lae1->requireVEnrichedCoupling(); }
      bool requireUVVolumePostSkeleton() const
      { return lae0->requireUVVolumePostSkeleton() || lae1->requireUVVolumePostSkeleton();}
      bool requireVVolumePostSkeleton() const
      { return lae0->requireVVolumePostSkeleton() || lae1->requireVVolumePostSkeleton(); }
      //! @}


      //! Public access to the wrapping local assembler
      const LocalAssembler & localAssembler(){ return la; }

      //! Set current solution vector. Must be called before
      //! setResidual(). Should be called prior to assembling.
      void setSolution(const Solution & solution_){
        solution = &solution_;
      }

      //! Set current residual vector. Should be called prior to
      //! assembling.
      void setJacobian(Jacobian & jacobian_){
        jacobian = &jacobian_;

        assert(solution != invalid_solution);

        // Initialize the engines of the two wrapped local assemblers
        lae0 = & la.la0.localJacobianAssemblerEngine(*jacobian,*solution);
        lae1 = & la.la1.localJacobianAssemblerEngine(*jacobian,*solution);
      }

      //! Called immediately after binding of local function space in
      //! global assembler.
      //! @{
      template<typename EG, typename LFSU, typename LFSV>
      void onBindLFSUV(const EG & eg, const LFSU & lfsu, const LFSV & lfsv){
        lae0->onBindLFSUV(eg,lfsu,lfsv);
        lae1->onBindLFSUV(eg,lfsu,lfsv);
      }

      template<typename EG, typename LFSV>
      void onBindLFSV(const EG & eg, const LFSV & lfsv){
        lae0->onBindLFSV(eg,lfsv);
        lae1->onBindLFSV(eg,lfsv);
      }

      template<typename IG, typename LFSU, typename LFSV>
      void onBindLFSUVInside(const IG & ig, const LFSU & lfsu, const LFSV & lfsv){
        lae0->onBindLFSUVInside(ig,lfsu,lfsv);
        lae1->onBindLFSUVInside(ig,lfsu,lfsv);
      }

      template<typename IG, typename LFSU, typename LFSV>
      void onBindLFSUVOutside(const IG & ig, const LFSU & lfsun, const LFSV & lfsvn){
        lae0->onBindLFSUVOutside(ig,lfsun,lfsvn);
        lae1->onBindLFSUVOutside(ig,lfsun,lfsvn);
      }

      template<typename IG, typename LFSV>
      void onBindLFSVInside(const IG & ig, const LFSV & lfsv){
        lae0->onBindLFSVInside(ig,lfsv);
        lae1->onBindLFSVInside(ig,lfsv);
      }

      template<typename IG, typename LFSV>
      void onBindLFSVOutside(const IG & ig, const LFSV & lfsvn){
        lae0->onBindLFSVOutside(ig,lfsvn);
        lae1->onBindLFSVOutside(ig,lfsvn);
      }

      template<typename LFSU, typename LFSV>
      void onBindLFSUVCoupling(const LFSU & lfsu, const LFSV & lfsv){}
      template<typename LFSU, typename LFSV>
      void onUnbindLFSUVCoupling(const LFSU & lfsu, const LFSV & lfsv){}
      template<typename LFSV>
      void onBindLFSVCoupling(const LFSV & lfsv){}
      template<typename LFSV>
      void onUnbindLFSVCoupling(const LFSV & lfsv){}

      //! @}

      //! Called when the local function space is about to be rebound or
      //! discarded 
      //! @{
      template<typename EG, typename LFSU, typename LFSV>
      void onUnbindLFSUV(const EG & eg, const LFSU & lfsu, const LFSV & lfsv){
        lae0->onUnbindLFSUV(eg,lfsu,lfsv);
        lae1->onUnbindLFSUV(eg,lfsu,lfsv);
      }

      template<typename EG, typename LFSV>
      void onUnbindLFSV(const EG & eg, const LFSV & lfsv){
        lae0->onUnbindLFSV(eg,lfsv);
        lae1->onUnbindLFSV(eg,lfsv);
      }

      template<typename IG, typename LFSU, typename LFSV>
      void onUnbindLFSUVInside(const IG & ig, const LFSU & lfsu, const LFSV & lfsv){
        lae0->onUnbindLFSUVInside(ig,lfsu,lfsv);
        lae1->onUnbindLFSUVInside(ig,lfsu,lfsv);
      }

      template<typename IG, typename LFSU, typename LFSV>
      void onUnbindLFSUVOutside(const IG & ig, const LFSU & lfsun, const LFSV & lfsvn){
        lae0->onUnbindLFSUVOutside(ig,lfsun,lfsvn);
        lae1->onUnbindLFSUVOutside(ig,lfsun,lfsvn);
      }

      template<typename IG, typename LFSV>
      void onUnbindLFSVInside(const IG & ig, const LFSV & lfsv){
        lae0->onUnbindLFSVInside(ig,lfsv);
        lae1->onUnbindLFSVInside(ig,lfsv);
      }

      template<typename IG, typename LFSV>
      void onUnbindLFSVOutside(const IG & ig, const LFSV & lfsvn){
        lae0->onUnbindLFSVOutside(ig,lfsvn);
        lae1->onUnbindLFSVOutside(ig,lfsvn);
      }

      //! @}

      //! Methods for loading of the local function's
      //! coefficients. These methods are blocked. The loading of the
      //! coefficients is done in each assemble call.
      //!@{
      template<typename LFSU>
      void loadCoefficientsLFSUInside(const LFSU & lfsu_s){
        lae0->loadCoefficientsLFSUInside(lfsu_s);
        lae1->loadCoefficientsLFSUInside(lfsu_s);
      }
      template<typename LFSU>
      void loadCoefficientsLFSUOutside(const LFSU & lfsu_n){
        lae0->loadCoefficientsLFSUOutside(lfsu_n);
        lae1->loadCoefficientsLFSUOutside(lfsu_n);
      }
      template<typename LFSU>
      void loadCoefficientsLFSUCoupling(const LFSU & lfsu_c){
        lae0->loadCoefficientsLFSUInside(lfsu_c);
        lae1->loadCoefficientsLFSUInside(lfsu_c);
      }
      //! @}


      //! Notifier functions, called immediately before and after assembling
      //! @{
      void preAssembly()
      {
        lae0->preAssembly();
        lae1->preAssembly();

        // Extract the coefficients of the time step scheme
        b_rr = la.osp_method->b(la.stage,la.stage);
        d_r = la.osp_method->d(la.stage);

        // Here we only want to know whether this stage is implicit
        implicit = std::abs(b_rr) > 1e-6;

        // prepare local operators for stage
        la.la0.setTime(la.time + d_r * la.dt);
        la.la1.setTime(la.time + d_r * la.dt);
        
        // Set weights
        la.la0.setWeight(b_rr * la.dt_factor0);
        la.la1.setWeight(la.dt_factor1);
      }

      void postAssembly(){
        lae0->postAssembly();
        lae1->postAssembly();
      }
      //! @}

      //! Assembling methods
      //! @{

      template<typename EG>
      bool assembleCell(const EG & eg)
      {
        bool rv = true;
        rv &= lae0->assembleCell(eg);
        rv &= lae1->assembleCell(eg);
        return rv;
      }

      template<typename EG, typename LFSU, typename LFSV>
      void assembleUVVolume(const EG & eg, const LFSU & lfsu, const LFSV & lfsv)
      {
        if(implicit)
          lae0->assembleUVVolume(eg,lfsu,lfsv);
        lae1->assembleUVVolume(eg,lfsu,lfsv);
      }
      
      template<typename EG, typename LFSV>
      void assembleVVolume(const EG & eg, const LFSV & lfsv)
      {
        if(implicit)
          lae0->assembleVVolume(eg,lfsv);
        lae1->assembleVVolume(eg,lfsv);
      }

      template<typename IG, typename LFSU_S, typename LFSV_S, typename LFSU_N, typename LFSV_N>
      void assembleUVSkeleton(const IG & ig, const LFSU_S & lfsu_s, const LFSV_S & lfsv_s,
                              const LFSU_N & lfsu_n, const LFSV_N & lfsv_n)
      {
        if(implicit)
          lae0->assembleUVSkeleton(ig,lfsu_s,lfsv_s,lfsu_n,lfsv_n);
      }

      template<typename IG, typename LFSV_S, typename LFSV_N>
      void assembleVSkeleton(const IG & ig, const LFSV_S & lfsv_s, const LFSV_N & lfsv_n)
      {
        if(implicit)
          lae0->assembleVSkeleton(ig,lfsv_s,lfsv_n);
      }

      template<typename IG, typename LFSU_S, typename LFSV_S>
      void assembleUVBoundary(const IG & ig, const LFSU_S & lfsu_s, const LFSV_S & lfsv_s)
      {
        if(implicit)
          lae0->assembleUVBoundary(ig,lfsu_s,lfsv_s);
      }

      template<typename IG, typename LFSV_S>
      void assembleVBoundary(const IG & ig, const LFSV_S & lfsv_s)
      {
        if(implicit)
          lae0->assembleVBoundary(ig,lfsv_s);
      }

      template<typename IG, typename LFSU_S, typename LFSV_S>
      void assembleUVProcessor(const IG & ig, const LFSU_S & lfsu_s, const LFSV_S & lfsv_s)
      {
        if(implicit)
          lae0->assembleUVProcessor(ig,lfsu_s,lfsv_s);
      }

      template<typename IG, typename LFSV_S>
      void assembleVProcessor(const IG & ig, const LFSV_S & lfsv_s)
      {
        if(implicit)
          lae0->assembleVProcessor(ig,lfsv_s);
      }

      template<typename IG, typename LFSU_S, typename LFSV_S, typename LFSU_N, typename LFSV_N,
               typename LFSU_C, typename LFSV_C>
      void assembleUVEnrichedCoupling(const IG & ig,
                                             const LFSU_S & lfsu_s, const LFSV_S & lfsv_s,
                                             const LFSU_N & lfsu_n, const LFSV_N & lfsv_n,
                                             const LFSU_C & lfsu_c, const LFSV_C & lfsv_c)
      {
        if(implicit)
          lae0->assembleUVEnrichedCoupling(ig,lfsu_s,lfsv_s,lfsu_n,lfsv_n,lfsu_c,lfsv_c);
      }

      template<typename IG, typename LFSV_S, typename LFSV_N, typename LFSV_C>
      void assembleVEnrichedCoupling(const IG & ig,
                                            const LFSV_S & lfsv_s,
                                            const LFSV_N & lfsv_n,
                                            const LFSV_C & lfsv_c) 
      {
        if(implicit)
          lae0->assembleVEnrichedCoupling(ig,lfsv_s,lfsv_n,lfsv_c);
      }

      template<typename EG, typename LFSU, typename LFSV>
      void assembleUVVolumePostSkeleton(const EG & eg, const LFSU & lfsu, const LFSV & lfsv)
      {
        if(implicit)
          lae0->assembleUVVolumePostSkeleton(eg,lfsu,lfsv);
      }

      template<typename EG, typename LFSV>
      void assembleVVolumePostSkeleton(const EG & eg, const LFSV & lfsv)
      {
        if(implicit)
          lae0->assembleVVolumePostSkeleton(eg,lfsv);
      }
      //! @}

    private:

      //! Reference to the wrapping local assembler object which
      //! constructed this engine
      const LocalAssembler & la;

      JacobianEngineDT0 * const invalid_lae0;
      JacobianEngineDT0 * lae0;
      JacobianEngineDT1 * const invalid_lae1;
      JacobianEngineDT1 * lae1;

      //! Default value indicating an invalid residual pointer
      Jacobian * const invalid_jacobian;

      //! Default value indicating an invalid solution pointer
      Solution * const invalid_solution;

      //! Pointer to the current constant part residual vector in
      //! which to assemble
      Jacobian * jacobian;

      //! Pointer to the current residual vector in which to assemble
      const Solution * solution;

      //! Coefficients of time stepping scheme
      Real b_rr, d_r;
      bool implicit;

    }; // End of class OneStepLocalJacobianAssemblerEngine

  };
};
#endif
