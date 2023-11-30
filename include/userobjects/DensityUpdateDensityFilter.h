//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ElementUserObject.h"
#include "MeshMetaDataInterface.h"
#include "MooseTypes.h"

/**
 * Element user object that performs SIMP optimization using a bisection algorithm using a volume
 * constraint.
 * ONLY USE AT timestep_end TO EXECUTE AFTER SensitivityFilter
 */
class DensityUpdateDensityFilter : public ElementUserObject
{
public:
  static InputParameters validParams();

  DensityUpdateDensityFilter(const InputParameters & parameters);

  virtual void initialize() override;
  virtual void timestepSetup() override{};
  virtual void execute() override;
  virtual void finalize() override{};
  virtual void threadJoin(const UserObject &) override{};

protected:
  /// The system mesh
  const MooseMesh & _mesh;
  /// Name of the mesh generator to get MeshMetaData from
  const MeshGeneratorName _mesh_generator;
  /// The name of the pseudo-density variable
  const VariableName _design_density_name;
  /// The elasticity compliance sensitivity name
  const VariableName _compliance_sensitivity_name;
  /// The pseudo-density variable
  MooseWritableVariable * _design_density;
  /// The filtered compliance sensitivity variable
  const MooseWritableVariable * _compliance_sensitivity;
  /// The volume fraction to be enforced
  const Real _volume_fraction;
  /// Cut-off radius for filtering
  const Real _radius;
  /// Number of elements in X direction
  const unsigned int _nx;
  /// Number of elements in Y direction
  const unsigned int _ny;
  /// Lower X Coordinate of the generated mesh
  const Real _xmin;
  /// Upper X Coordinate of the generated mesh
  const Real _xmax;
  /// Lower Y Coordinate of the generated mesh
  const Real _ymin;
  /// Upper Y Coordinate of the generated mesh
  const Real _ymax;

private:
  struct ElementData
  {
    Real old_density;
    Real sensitivity;
    Real volume;
    Real new_density;
    ElementData() = default;
    ElementData(Real dens, Real sens, Real vol, Real filt_dens)
      : old_density(dens), sensitivity(sens), volume(vol), new_density(filt_dens)
    {
    }
  };

  /**
   * Gathers element date necessary to perform the bisection algorithm for optimization
   */
  void gatherElementData();

  /**
   * Prepares data needed for filtering of new densitys
   */
  void prepareFilter();

  /**
   * Performs the opti<mality criterion loop (bisection)
   */
  void performOptimCritLoop();

  Real computeUpdatedDensity(Real current_density, Real dc, Real lmid);

  /// Total volume allowed for volume contraint
  Real _total_allowable_volume;

  /// Data structure to hold old density, sensitivity, volume, current density.
  std::map<dof_id_type, ElementData> _elem_data_map;

  /// Data structures to hold the filter weights
  std::vector<std::vector<Real>> _H;
  std::vector<Real> _Hs;

  /// Lower bound for bisection algorithm
  const Real _lower_bound;
  /// Upper bound for bisection algorithm
  const Real _upper_bound;
};
