// -*- tab-width: 4; indent-tabs-mode: nil -*-
#ifndef DUNE_PDELAB_GRIDOPERATORSPACE_HH
#define DUNE_PDELAB_GRIDOPERATORSPACE_HH

#include<map>

#include<dune/common/exceptions.hh>
#include<dune/common/geometrytype.hh>

#include"../common/geometrywrapper.hh"
#include"../gridfunctionspace/gridfunctionspace.hh"
#include"../gridfunctionspace/constraints.hh"
#include"localmatrix.hh"
#include"gridoperatorspaceutilities.hh"


namespace Dune {
  namespace PDELab {


	//================================================
	// The operator
	//================================================

	//! collect types exported by a grid operator space
	template<typename GFSU, typename GFSV, typename B, 
			 typename CU, typename CV>
	struct GridOperatorSpaceTraits
	{
	  typedef GFSU TrialGridFunctionSpace;

	  typedef CU TrialConstraintsType;

	  typedef GFSV TestGridFunctionSpace;

	  typedef CV TestConstraintsType;

	  //! \brief the grid view where grid function is defined upon
	  typedef typename GFSU::Traits::GridViewType GridViewType;

	  //! \brief vector backend
	  typedef B BackendType;

	  //! \brief short cut for size type exported by Backend
	  typedef typename B::size_type SizeType;
	};


    class EmptyTransformation : public ConstraintsTransformation<int,float>
    {
    };

	// The generic assembler ...
	// GFSU, GFSV : grid function spaces
	// LP : local pattern assembler (provided by user)
	// LA : local operator assembler (provided by user)
	template<typename GFSU, typename GFSV, typename LA,
			 typename CU=EmptyTransformation,
			 typename CV=EmptyTransformation,
			 typename B=StdVectorFlatMatrixBackend>
	class GridOperatorSpace 
	{
	  // extract useful types
	  typedef typename GFSU::Traits::GridViewType GV;
      typedef typename GV::Traits::template Codim<0>::Iterator ElementIterator;
	  typedef typename GV::Traits::template Codim<0>::Entity Element;
	  typedef typename GV::IntersectionIterator IntersectionIterator;
	  typedef typename IntersectionIterator::Intersection Intersection;

	public:
	  typedef GridOperatorSpaceTraits<GFSU,GFSV,B,CU,CV> Traits;

      template<typename E>
      struct MatrixContainer
      {
        //! \brief define Type as the Type of a Matrix of E's
        typedef typename B::template Matrix<GridOperatorSpace,E> Type;
      private:
        MatrixContainer () {}
      };

	  GridOperatorSpace (const GFSU& gfsu_, const GFSV& gfsv_, const LA& la_) 
		: gfsu(gfsu_), gfsv(gfsv_), la(la_)
	  {
		pconstraintsu = &emptyconstraintsu;
		pconstraintsv = &emptyconstraintsv;
	  }

	  GridOperatorSpace (const GFSU& gfsu_, const CU& cu,
						 const GFSV& gfsv_, const CV& cv,
						 const LA& la_) 
		: gfsu(gfsu_), gfsv(gfsv_), la(la_)
	  {
		pconstraintsu = &cu;
		pconstraintsv = &cv;
	  }

      //! get dimension of space u
	  typename GFSU::Traits::SizeType globalSizeU () const
	  {
		return gfsu.globalSize();
	  }

      //! get dimension of space v
	  typename GFSV::Traits::SizeType globalSizeV () const
	  {
		return gfsv.globalSize();
	  }


      /**\brief Construct global sparsity pattern from local description

         This function can be called by the Matrix to get the sparsity pattern. 
         Assumes that the pattern is initially empty.
      */
      template<typename P>
      void fill_pattern (P& globalpattern) const
      {
 		// make local function spaces
		typedef typename GFSU::LocalFunctionSpace LFSU;
		LFSU lfsu(gfsu);
		typedef typename GFSV::LocalFunctionSpace LFSV;
		LFSV lfsv(gfsv);

        for (ElementIterator it = gfsu.gridview().template begin<0>();
             it!=gfsu.gridview().template end<0>(); ++it)
          {
 			// bind local function spaces to element
			lfsu.bind(*it);
			lfsv.bind(*it);

            // get local pattern of operator
            {
              LocalSparsityPattern localpattern;
              LocalAssemblerCallSwitch<LA,LA::doPatternVolume>::
                pattern_volume(la,lfsu,lfsv,localpattern);
              
              // translate local to global indices and add to global pattern
              for (size_t k=0; k<localpattern.size(); ++k)
                add_entry(globalpattern,
                          lfsv.globalIndex(localpattern[k].i()),
                          lfsu.globalIndex(localpattern[k].j())
                          );
            }

            // skeleton and boundary pattern
            if (!LA::doPatternSkeleton) continue;

            // local function spaces in neighbor
            LFSU lfsun(gfsu);
            LFSV lfsvn(gfsv);

            IntersectionIterator endit = gfsu.gridview().iend(*it);
            for (IntersectionIterator iit = gfsu.gridview().ibegin(*it); 
                 iit!=endit; ++iit)
              {
                // skip if there is no neighbor
                if (!iit->neighbor()) continue;
                
                // bind local function spaces to neighbor element
                lfsun.bind(*(iit->outside()));
                lfsvn.bind(*(iit->outside()));
                
                // get pattern
                LocalSparsityPattern localpattern_sn, localpattern_ns;
                LocalAssemblerCallSwitch<LA,LA::doPatternSkeleton>::
                  pattern_skeleton(la,lfsu,lfsv,lfsun,lfsvn,localpattern_sn,localpattern_ns);

                // translate local to global indices and add to global pattern
                for (size_t k=0; k<localpattern_sn.size(); ++k)
                  add_entry(globalpattern,
                            lfsv.globalIndex(localpattern_sn[k].i()),
                            lfsun.globalIndex(localpattern_sn[k].j())
                            );

                for (size_t k=0; k<localpattern_ns.size(); ++k)
                  add_entry(globalpattern,
                            lfsvn.globalIndex(localpattern_ns[k].i()),
                            lfsu.globalIndex(localpattern_ns[k].j())
                            );
			  }
          }
      }


	  // generic evaluation of residual
	  template<typename X, typename R> 
	  void residual (const X& x, R& r) const
	  {
        // visit each face only once
        const int chunk=1<<28;
        int offset = 0;
        const typename GV::IndexSet& is=gfsu.gridview().indexSet();
        std::map<Dune::GeometryType,int> gtoffset;

		// make local function spaces
		typedef typename GFSU::LocalFunctionSpace LFSU;
		LFSU lfsu(gfsu);
		typedef typename GFSV::LocalFunctionSpace LFSV;
		LFSV lfsv(gfsv);

		// traverse grid view
		for (ElementIterator it = gfsu.gridview().template begin<0>();
			 it!=gfsu.gridview().template end<0>(); ++it)
		  {
            // assign offset for geometry type;
            if (gtoffset.find(it->type())==gtoffset.end())
              {
                gtoffset[it->type()] = offset;
                offset += chunk;
              }

            // compute unique id
            int id = is.index(*it)+gtoffset[it->type()];

			// bind local function spaces to element
			lfsu.bind(*it);
			lfsv.bind(*it);

			// allocate local data container
			std::vector<typename X::ElementType> xl(lfsu.size());
			std::vector<typename R::ElementType> rl(lfsv.size(),0.0);

			// read coefficents
			lfsu.vread(x,xl);

			// volume evaluation
			LocalAssemblerCallSwitch<LA,LA::doAlphaVolume>::
              alpha_volume(la,ElementGeometry<Element>(*it),lfsu,xl,lfsv,rl);
			LocalAssemblerCallSwitch<LA,LA::doLambdaVolume>::
              lambda_volume(la,ElementGeometry<Element>(*it),lfsv,rl);

			// skip if no intersection iterator is needed
 			if (LA::doAlphaSkeleton||LA::doAlphaBoundary||LA::doLambdaSkeleton||LA::doLambdaBoundary)
              {
                // local function spaces in neighbor
                LFSU lfsun(gfsu);
                LFSV lfsvn(gfsv);

                // traverse intersections
                IntersectionIterator endit = gfsu.gridview().iend(*it);
                for (IntersectionIterator iit = gfsu.gridview().ibegin(*it); 
                     iit!=endit; ++iit)
                  {
                    // skeleton term
                    if (iit->neighbor() && (LA::doAlphaSkeleton||LA::doLambdaSkeleton) )
                      {
                        // assign offset for geometry type;
                        Dune::GeometryType gtn = iit->outside()->type();
                        if (gtoffset.find(gtn)==gtoffset.end())
                          {
                            gtoffset[gtn] = offset;
                            offset += chunk;
                          }
                        
                        // compute unique id for neighbor
                        int idn = is.index(*(iit->outside()))+gtoffset[gtn];
                          
                        // unique vist of intersection
                        if (id>idn)
                          {
                            // bind local function spaces to neighbor element
                            lfsun.bind(*(iit->outside()));
                            lfsvn.bind(*(iit->outside()));
                            
                            // allocate local data container
                            std::vector<typename X::ElementType> xn(lfsun.size());
                            std::vector<typename R::ElementType> rn(lfsvn.size(),0.0);
                            
                            // read coefficents
                            lfsun.vread(x,xn);
                            
                            // skeleton evaluation
                            LocalAssemblerCallSwitch<LA,LA::doAlphaSkeleton>::
                              alpha_skeleton(la,IntersectionGeometry<Intersection>(*iit),lfsu,xl,lfsv,lfsun,xn,lfsvn,rl,rn);
                            
                            // accumulate result (note: r needs to be cleared outside)
                            lfsvn.vadd(rn,r);
                          }
                      }
               
                    // boundary term
                    if (iit->boundary())
                      {
                        LocalAssemblerCallSwitch<LA,LA::doAlphaBoundary>::
                          alpha_boundary(la,IntersectionGeometry<Intersection>(*iit),lfsu,xl,lfsv,rl);
                        LocalAssemblerCallSwitch<LA,LA::doLambdaBoundary>::
                          lambda_boundary(la,IntersectionGeometry<Intersection>(*iit),lfsv,rl);
                      }
                  }
              }

			// accumulate result (note: r needs to be cleared outside)
			lfsv.vadd(rl,r);
		  }

		// set residual to zero on constrained dofs
		Dune::PDELab::constrain_residual(*pconstraintsv,r);
	  }

	  // generic evaluation of residual
	  template<typename X, typename Y> 
	  void jacobian_apply (X& x, Y& y) const
	  {
        // visit each face only once
        const int chunk=1<<28;
        int offset = 0;
        const typename GV::IndexSet& is=gfsu.gridview().indexSet();
        std::map<Dune::GeometryType,int> gtoffset;

		// make local function spaces
		typedef typename GFSU::LocalFunctionSpace LFSU;
		LFSU lfsu(gfsu);
		typedef typename GFSV::LocalFunctionSpace LFSV;
		LFSV lfsv(gfsv);

		// traverse grid view
		for (ElementIterator it = gfsu.gridview().template begin<0>();
			 it!=gfsu.gridview().template end<0>(); ++it)
		  {
            // assign offset for geometry type;
            if (gtoffset.find(it->type())==gtoffset.end())
              {
                gtoffset[it->type()] = offset;
                offset += chunk;
              }

            // compute unique id
            int id = is.index(*it)+gtoffset[it->type()];

			// bind local function spaces to element
			lfsu.bind(*it);
			lfsv.bind(*it);

			// allocate local data container
			std::vector<typename X::ElementType> xl(lfsu.size());
			std::vector<typename Y::ElementType> yl(lfsv.size(),0.0);

			// read coefficents
			lfsu.vread(x,xl);

			// volume evaluation
			LocalAssemblerCallSwitch<LA,LA::doAlphaVolume>::
			  jacobian_apply_volume(la,ElementGeometry<Element>(*it),lfsu,xl,lfsv,yl);

			// skeleton and boundary evaluation
			if (LA::doAlphaSkeleton||LA::doAlphaBoundary)
			  {
                // local function spaces in neighbor
                LFSU lfsun(gfsu);
                LFSV lfsvn(gfsv);

				IntersectionIterator endit = gfsu.gridview().iend(*it);
				for (IntersectionIterator iit = gfsu.gridview().ibegin(*it); 
					 iit!=endit; ++iit)
				  {
                    // skeleton term
                    if (iit->neighbor() && LA::doAlphaSkeleton )
                      {
                        // assign offset for geometry type;
                        Dune::GeometryType gtn = iit->outside()->type();
                        if (gtoffset.find(gtn)==gtoffset.end())
                          {
                            gtoffset[gtn] = offset;
                            offset += chunk;
                          }
                        
                        // compute unique id for neighbor
                        int idn = is.index(*(iit->outside()))+gtoffset[gtn];
                          
                        // unique vist of intersection
                        if (id>idn)
                          {
                            // bind local function spaces to neighbor element
                            lfsun.bind(*(iit->outside()));
                            lfsvn.bind(*(iit->outside()));
                            
                            // allocate local data container
                            std::vector<typename X::ElementType> xn(lfsun.size());
                            std::vector<typename Y::ElementType> yn(lfsvn.size(),0.0);
                            
                            // read coefficents
                            lfsun.vread(x,xn);
                            
                            // skeleton evaluation
                            LocalAssemblerCallSwitch<LA,LA::doAlphaSkeleton>::
                              jacobian_apply_skeleton(la,IntersectionGeometry<Intersection>(*iit),lfsu,xl,lfsv,lfsun,xn,lfsvn,yl,yn);

                            // accumulate result (note: r needs to be cleared outside)
                            lfsvn.vadd(yn,y);
                          }
                      }

                    // boundary term
                    if (iit->boundary())
                      {
                        LocalAssemblerCallSwitch<LA,LA::doAlphaBoundary>::
                          jacobian_apply_boundary(la,IntersectionGeometry<Intersection>(*iit),lfsu,xl,lfsv,yl);
                      }
				  }
			  }

			// accumulate result (note: r needs to be cleared outside)
			lfsv.vadd(yl,y);
		  }

		// set residual to zero on constrained dofs
		Dune::PDELab::copy_constrained_dofs(*pconstraintsu,x,y);
	  }

	  // generic assembly of Jacobian
	  template<typename X, typename A> 
	  void jacobian (const X& x, A& a) const
	  {
        // visit each face only once
        const int chunk=1<<28;
        int offset = 0;
        const typename GV::IndexSet& is=gfsu.gridview().indexSet();
        std::map<Dune::GeometryType,int> gtoffset;

		// make local function spaces
		typedef typename GFSU::LocalFunctionSpace LFSU;
		LFSU lfsu(gfsu);
		typedef typename GFSV::LocalFunctionSpace LFSV;
		LFSV lfsv(gfsv);

		// traverse grid view
		for (ElementIterator it = gfsu.gridview().template begin<0>();
			 it!=gfsu.gridview().template end<0>(); ++it)
		  {
            // assign offset for geometry type;
            if (gtoffset.find(it->type())==gtoffset.end())
              {
                gtoffset[it->type()] = offset;
                offset += chunk;
              }

            // compute unique id
            int id = is.index(*it)+gtoffset[it->type()];

			// bind local function spaces to element
			lfsu.bind(*it);
			lfsv.bind(*it);

			// allocate local data container
			std::vector<typename X::ElementType> xl(lfsu.size());
			LocalMatrix<typename A::ElementType> al(lfsv.size(),lfsu.size(),0.0);

			// read coefficents
			lfsu.vread(x,xl);

			// volume evaluation
			LocalAssemblerCallSwitch<LA,LA::doAlphaVolume>::
			  jacobian_volume(la,ElementGeometry<Element>(*it),lfsu,xl,lfsv,al);

			// skeleton and boundary evaluation
			if (LA::doAlphaSkeleton||LA::doAlphaBoundary)
			  {
                // local function spaces in neighbor
                LFSU lfsun(gfsu);
                LFSV lfsvn(gfsv);
                
				IntersectionIterator endit = gfsu.gridview().iend(*it);
				for (IntersectionIterator iit = gfsu.gridview().ibegin(*it); 
					 iit!=endit; ++iit)
				  {
                    // skeleton term
                    if (iit->neighbor() && LA::doAlphaSkeleton )
                      {
                        // assign offset for geometry type;
                        Dune::GeometryType gtn = iit->outside()->type();
                        if (gtoffset.find(gtn)==gtoffset.end())
                          {
                            gtoffset[gtn] = offset;
                            offset += chunk;
                          }
                        
                        // compute unique id for neighbor
                        int idn = is.index(*(iit->outside()))+gtoffset[gtn];
                          
                        // unique vist of intersection
                        if (id>idn)
                          {
                            // bind local function spaces to neighbor element
                            lfsun.bind(*(iit->outside()));
                            lfsvn.bind(*(iit->outside()));
                            
                            // allocate local data container
                            std::vector<typename X::ElementType> xn(lfsun.size());
                            LocalMatrix<typename A::ElementType> al_sn(lfsv.size() ,lfsun.size(),0.0);
                            LocalMatrix<typename A::ElementType> al_ns(lfsvn.size(),lfsu.size() ,0.0);
                            LocalMatrix<typename A::ElementType> al_nn(lfsvn.size(),lfsun.size(),0.0);
                            
                            // read coefficents
                            lfsun.vread(x,xn);
                            
                            // skeleton evaluation
                            LocalAssemblerCallSwitch<LA,LA::doAlphaSkeleton>::
                              jacobian_skeleton(la,IntersectionGeometry<Intersection>(*iit),
                                                lfsu,xl,lfsv,lfsun,xn,lfsvn,al,al_sn,al_ns,al_nn);

                            // accumulate result
                            etadd(lfsv,lfsun,al_sn,a);
                            etadd(lfsvn,lfsu,al_ns,a);
                            etadd(lfsvn,lfsun,al_nn,a);
                          }
                      }

                    // boundary term
                    if (iit->boundary())
                      {
                        LocalAssemblerCallSwitch<LA,LA::doAlphaBoundary>::
                          jacobian_boundary(la,IntersectionGeometry<Intersection>(*iit),lfsu,xl,lfsv,al);
                      }
				  }
			  }

			// accumulate result (note: r needs to be cleared outside)
			etadd(lfsv,lfsu,al,a);
		  }

		
         typedef typename CV::const_iterator global_row_iterator;	  
         for (global_row_iterator cit=pconstraintsv->begin(); cit!=pconstraintsv->end(); ++cit)
           set_trivial_row(cit->first,cit->second,a);
 	  }

      /** \brief Transforms a vector /f$ \boldsymbol{x} /f$ from /f$
          V/f$ to /f$ V'/f$. If postrestrict == true then
          /f$\boldsymbol{R}^T_{\tilde \boldsymbol{U}', \boldsymbol{U}'}
          \boldsymbol{S}_{\tilde \boldsymbol{V}}/f$ is applied
           instead of the full transformation.  */
      template<typename X>
      void forwardtransform(X & x, const bool postrestrict = false)
      {
        typedef typename CV::const_iterator global_col_iterator;	  
        for (global_col_iterator cit=pconstraintsv->begin(); cit!=pconstraintsv->end(); ++cit){
          typedef typename global_col_iterator::value_type::first_type GlobalIndex;
          const GlobalIndex & contributor = cit->first;

          typedef typename global_col_iterator::value_type::second_type ContributedMap;
          typedef typename ContributedMap::const_iterator global_row_iterator;
          const ContributedMap & contributed = cit->second;
          global_row_iterator it  = contributed.begin();
          global_row_iterator eit = contributed.end();
          
          for(;it!=eit;++it)
            x[it->first] += it->second * x[contributor];
        }

        if(postrestrict)
          for (global_col_iterator cit=pconstraintsv->begin(); cit!=pconstraintsv->end(); ++cit)
            x[cit->first]=0.;
      }

      /** \brief Transforms a vector /f$ \boldsymbol{x} /f$ from /f$
          V'/f$ to /f$ V/f$. If prerestrict == true then
          /f$\boldsymbol{S}^T_{\tilde \boldsymbol{U}}/f$ is applied
           instead of the full transformation.  */
      template<typename X>
      void backtransform(X & x, const bool prerestrict = false)
      {
        typedef typename CV::const_iterator global_col_iterator;	  
        for (global_col_iterator cit=pconstraintsv->begin(); cit!=pconstraintsv->end(); ++cit){
          typedef typename global_col_iterator::value_type::first_type GlobalIndex;
          const GlobalIndex & contributor = cit->first;

          typedef typename global_col_iterator::value_type::second_type ContributedMap;
          typedef typename ContributedMap::const_iterator global_row_iterator;
          const ContributedMap & contributed = cit->second;
          global_row_iterator it  = contributed.begin();
          global_row_iterator eit = contributed.end();
          
          if(prerestrict)
            x[contributor] = 0.;

          for(;it!=eit;++it)
            x[contributor] += it->second * x[it->first];
        }
      }

	private:

      /** \brief read local stiffness matrix for entity */  
      template<typename LFSV, typename LFSU, typename GC, typename T>
      void eread (const LFSV& lfsv, const LFSU& lfsu, const GC& globalcontainer, 
                  LocalMatrix<T>& localcontainer) const
      {
        for (int i=0; i<lfsv.size(); i++)
          for (int j=0; j<lfsu.size(); j++)
            localcontainer(i,j) = B::const_access(globalcontainer,lfsv.globalIndex(i),lfsu.globalIndex(j));
      }

      /** \brief write local stiffness matrix for entity */  
      template<typename LFSV, typename LFSU, typename T, typename GC>
      void ewrite (const LFSV& lfsv, const LFSU& lfsu, const LocalMatrix<T>& localcontainer, GC& globalcontainer) const
      {
        for (int i=0; i<lfsv.size(); i++)
          for (int j=0; j<lfsu.size(); j++)
            B::access(globalcontainer,lfsv.globalIndex(i),lfsu.globalIndex(j)) = localcontainer(i,j);
      }

      /** \brief write local stiffness matrix for entity */  
      template<typename LFSV, typename LFSU, typename T, typename GC>
      void eadd (const LFSV& lfsv, const LFSU& lfsu, const LocalMatrix<T>& localcontainer, GC& globalcontainer) const
      {
        for (size_t i=0; i<lfsv.size(); i++)
          for (size_t j=0; j<lfsu.size(); j++)
            B::access(globalcontainer,lfsv.globalIndex(i),lfsu.globalIndex(j)) += localcontainer(i,j);
      }

      /** \brief Add local matrix /f$m/f$ to global Jacobian /f$J/f$
          and apply constraints transformation. Hence we perform: /f$
          \boldsymbol{J} := \boldsymbol{J} + \boldsymbol{S}_{\tilde
          \boldsymbol{V}} m \boldsymbol{S}^T_{\tilde
          \boldsymbol{\tilde U}} /f$*/  
      template<typename LFSV, typename LFSU, typename T, typename GC>
      void etadd (const LFSV& lfsv, const LFSU& lfsu, const LocalMatrix<T>& localcontainer, GC& globalcontainer) const
      {

        for (size_t i=0; i<lfsv.size(); i++)
          for (size_t j=0; j<lfsu.size(); j++){
            typename Traits::SizeType gi = lfsv.globalIndex(i);
            typename Traits::SizeType gj = lfsu.globalIndex(j);
            
            // Get global constraints containers for test and ansatz space
            const CV & cv = *pconstraintsv;
            const CU & cu = *pconstraintsu;

            typedef typename CV::const_iterator global_vcol_iterator;
            typedef typename global_vcol_iterator::value_type::second_type global_vrow_type;
            typedef typename global_vrow_type::const_iterator global_vrow_iterator;

            typedef typename CU::const_iterator global_ucol_iterator;
            typedef typename global_ucol_iterator::value_type::second_type global_urow_type;
            typedef typename global_urow_type::const_iterator global_urow_iterator;

            // Check whether the global indices are constrained indices
            global_vcol_iterator gvcit = cv.find(gi);
            global_ucol_iterator gucit = cu.find(gj);

            // Set constrained_v true if gi is constrained dof
            bool constrained_v(false);
            global_vrow_iterator gvrit;
            if(gvcit!=cv.end()){
              gvrit = gvcit->second.begin();              
              constrained_v = true;
            }

            double vf = 1;
            do{
              // if gi is index of constrained dof
              if(constrained_v){
                // then break if it is a dirichlet dof
                if(gvrit == gvcit->second.end())
                  break;

                // otherwise set gi to an index to a contributed dof
                // and set vf to the contribution weight
                gi = gvrit->first;
                vf = gvrit->second;
              }

            // Set constrained_u true if gj is constrained dof
              bool constrained_u(false);
              global_urow_iterator gurit;
              if(gucit!=cu.end()){
                gurit = gucit->second.begin();
                constrained_u = true;
              }

              double uf = 1;
              do{
                // if gj is index of constrained dof
                if(constrained_u){
                  // then break if it is a dirichlet dof
                  if(gurit == gucit->second.end())
                    break;

                  // otherwise set gj to an index to a contributed dof
                  // and set uf to the contribution weight
                  gj = gurit->first;
                  uf = gurit->second;
                }

                // add weighted local entry to global matrix
                B::access(globalcontainer,gi,gj) += localcontainer(i,j) * uf * vf;

                if(constrained_u && gurit != gucit->second.end())
                  ++gurit;
                else 
                  break;

              }while(true);

              if(constrained_v && gvrit != gvcit->second.end())
                ++gvrit;
              else
                break;

            }while(true);

          }
      }

      /** \brief Adding matrix entry to pattern with respect to the
       constraints contributions. This assembles the entries addressed
       by etadd(..). See the documentation there for more information
       about the matrix pattern. */
      template<typename GI, typename P>
      void add_entry(P & globalpattern, GI gi, GI gj) const
      {
        const CV & cv = *pconstraintsv;
        const CU & cu = *pconstraintsu;

        typedef typename CV::const_iterator global_vcol_iterator;
        typedef typename global_vcol_iterator::value_type::second_type global_vrow_type;
        typedef typename global_vrow_type::const_iterator global_vrow_iterator;

        typedef typename CU::const_iterator global_ucol_iterator;
        typedef typename global_ucol_iterator::value_type::second_type global_urow_type;
        typedef typename global_urow_type::const_iterator global_urow_iterator;
            
        global_vcol_iterator gvcit = cv.find(gi);
        global_ucol_iterator gucit = cu.find(gj);

        if(gi==gj)
          globalpattern.add_link(gi,gj);

        bool constrained_v(false);
        global_vrow_iterator gvrit;
        if(gvcit!=cv.end()){
          gvrit = gvcit->second.begin();              
          constrained_v = true;
        }

        do{
          if(constrained_v){
            if(gvrit == gvcit->second.end())
              break;
            gi = gvrit->first;
          }

          bool constrained_u(false);
          global_urow_iterator gurit;
          if(gucit!=cu.end()){
            gurit = gucit->second.begin();
            constrained_u = true;
          }

          do{
            if(constrained_u){
              if(gurit == gucit->second.end())
                break;
              gj = gurit->first;
            }
                
            globalpattern.add_link(gi,gj);

            if(constrained_u && gurit != gucit->second.end())
              ++gurit;
            else 
              break;

          }while(true);

          if(constrained_v && gvrit != gvcit->second.end())
            ++gvrit;
          else
            break;

        }while(true);

      }

      /** \brief insert dirichlet constraints for row and assemble
          T^T_U in constrained rows
      */  
      template<typename GI, typename GC, typename CG>
      void set_trivial_row (GI i, const CG & cv_i, GC& globalcontainer) const
      {
        // set all entries in row i to zero
         B::clear_row(i,globalcontainer);

        // set diagonal element to 1
        B::access(globalcontainer,i,i) = 1;
      }


	  const GFSU& gfsu;
	  const GFSV& gfsv;
	  const LA& la;
	  const CU* pconstraintsu;
	  const CV* pconstraintsv;
	  CU emptyconstraintsu;
	  CV emptyconstraintsv;
	};

    //! \} group GridFunctionSpace
  } // namespace PDELab
} // namespace Dune

#endif
