//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "TODesignResponse.h"
#include "MooseTypes.h"

/**
 * Element user object that provides basic functionality for stress constrained topology
 * optimization
 */
class StressResponseBase : public TODesignResponse
{
public:
  static InputParameters validParams();

  StressResponseBase(const InputParameters & parameters);

  virtual void initialSetup() override;
  virtual void initialize() override;
  virtual void execute() override;
  virtual void finalize() override{};
  virtual void threadJoin(const UserObject &) override{};

  virtual void gatherElementData() override;
  virtual void computeStress() = 0;
  virtual void computeValue() override{};
  virtual void computeSensitivity() override{};

protected:
  /// DOF map
  const DofMap & _dof_map;
  std::map<dof_id_type, std::vector<dof_id_type>> _elem_to_node_map;
  /// Stress variable names
  const std::vector<VariableName> _stress_names;
  /// Von Mises stress variable
  MooseVariable * _vonmises_stress;
  /// Stress_xx variable
  MooseVariable * _stress_xx;
  /// Stress_yy variable
  MooseVariable * _stress_yy;
  /// Stress_xy variable
  MooseVariable * _stress_xy;
  /// Voigt stress vector for every Element
  RealEigenMatrix _stress;
  /// Elemental Von Mises stress vector
  RealEigenVector _vonmises;
  /// Displacement variable names
  const std::vector<VariableName> _displacement_names;
  /// disp_x variable
  MooseVariable * _disp_x;
  /// disp_y variable
  MooseVariable * _disp_y;
  /// E0
  const Real _E0;
  /// Emin
  const Real _Emin;
  /// Poisson's ratio
  const Real _nu;
  /// Penalty value
  const Real _p;
  /// Aggregation parameter
  const Real _P;
  /// Elasticity matrix
  RealEigenMatrix _E;
  /// Element number
  unsigned int _n_el;
  /// Number of DOFs
  dof_id_type _n_dofs;
  /// Vector of all DOFs
  std::vector<dof_id_type> _all_dofs;
  /// Vector of fixed DOFs
  std::vector<dof_id_type> _fixed_dofs;
  /// Vector of free DOFs
  std::vector<dof_id_type> _free_dofs;
  /// Map of DOFs per elements
  std::vector<std::vector<dof_id_type>> _elem_to_dof_map;
  /// Element stiffness matrix
  RealEigenMatrix _KE;
  /// Global displacement vector
  RealEigenVector _U;

  struct ElementData
  {
    Real vonmises_stress;
    Real stress_xx;
    Real stress_yy;
    Real stress_xy;
    Real physical_density;
    Real stress_sensitivity;
    std::vector<dof_id_type> nodes;
    RealEigenVector u_el;
    ElementData() = default;
    ElementData(Real s_vm,
                Real xx,
                Real yy,
                Real xy,
                Real dens,
                Real ds,
                std::vector<dof_id_type> n,
                RealEigenVector u_el)
      : vonmises_stress(s_vm),
        stress_xx(xx),
        stress_yy(yy),
        stress_xy(xy),
        physical_density(dens),
        stress_sensitivity(ds),
        nodes(n),
        u_el(u_el)
    {
    }
  };

  struct NodalData
  {
    Real disp_x;
    Real disp_y;
    std::vector<dof_id_type> dofs;
    NodalData() = default;
    NodalData(Real ux, Real uy, std::vector<dof_id_type> dofs) : disp_x(ux), disp_y(uy), dofs(dofs)
    {
    }
  };

  /// Data structure to hold elemental values
  std::map<dof_id_type, ElementData> _elem_data_map;

  /// Data structure to hold nodal values
  std::map<dof_id_type, NodalData> _nodal_data_map;

  /**
   * Gathers nodal data necessary to calculate stress sensitivity
   */
  void gatherNodalData();

  /**
   * Initializes _n_dofs, _all_dofs, _fixed_dofs, _free_dofs and _elem_to_dof_map
   */
  void initializeDofVariables();

  /**
   * Initializes _E
   */
  void initializeEMat();

  /**
   * Returns B-matrix at coordinates xi,eta for regular element
   */
  RealEigenMatrix getBMat(Real xi, Real eta);

  /**
   * Initializes _KE
   */
  void initializeKeMat();

  /**
   * Initializes _U
   */
  void initializeUVec();

  /**
   * Copmutes lambda from system matrix and gamma_red
   */
  RealEigenVector getLambda(std::vector<Real> gamma_red);

  /**
   * Copmutes T2 from lambda
   */
  RealEigenVector getT2(RealEigenVector lambda);
};
