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
 * Element user object that computes the compliance sensitivity
 */
class ComplianceResponse : public TODesignResponse
{
public:
  static InputParameters validParams();

  ComplianceResponse(const InputParameters & parameters);

  virtual void initialSetup() override;
  virtual void initialize() override;
  virtual void execute() override;
  virtual void finalize() override {};
  virtual void threadJoin(const UserObject &) override {};

  virtual void gatherElementData() override;
  virtual void scaleConstraint() override {};
  virtual void computeValue();
  virtual void computeSensitivity();

protected:
  /// DOF map
  const DofMap & _dof_map;
  /// Name of the mesh generator to get MeshMetaData from
  MeshGeneratorName _mesh_generator;
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
  /// Elasticity matrix
  RealEigenMatrix _E;
  /// Number of elements in X direction
  unsigned int _nx;
  /// Number of elements in Y direction
  unsigned int _ny;
  /// Lower X Coordinate of the generated mesh
  Real _xmin;
  /// Upper X Coordinate of the generated mesh
  Real _xmax;
  /// Lower Y Coordinate of the generated mesh
  Real _ymin;
  /// Upper Y Coordinate of the generated mesh
  Real _ymax;
  /// Element size
  Real _l_el;
  /// Number of DOFs
  dof_id_type _n_dofs;
  /// Number of local DOFs
  dof_id_type _n_local_dofs;
  /// Vector of all DOFs
  std::vector<dof_id_type> _all_dofs;
  /// Vector of fixed DOFs
  std::vector<dof_id_type> _fixed_dofs;
  /// Map of DOFs per element
  std::vector<std::vector<dof_id_type>> _elem_to_dof_map;
  /// Map of DOFs per node
  std::vector<std::vector<dof_id_type>> _node_to_dof_map;
  /// Element stiffness matrix
  RealEigenMatrix _KE;
  /// Global displacement vector
  RealEigenVector _U;
  /// System matrix name
  std::string _system_matrix_name;

  struct ElementData
  {
    Real physical_density;
    Real compliance_sensitivity;
    ElementData() = default;
    ElementData(Real dens, Real dc) : physical_density(dens), compliance_sensitivity(dc) {}
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
  RealEigenMatrix computeBMat(Real xi, Real eta);

  /**
   * Initializes _KE
   */
  void initializeKeMat();

  /**
   * Initializes _U
   */
  void initializeUVec();
};
