//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "FilterBase.h"
#include "MooseTypes.h"

/**
 * Element user object that filters the objective function sensitivities via a radial average user
 * objects. This object can be used to apply a Solid Isotropic Material Penalization (SIMP) to
 * optimization.
 */
class HeavisideProjection : public FilterBase
{
public:
  static InputParameters validParams();

  HeavisideProjection(const InputParameters & parameters);

  virtual void initialSetup() override;
  virtual void initialize() override;
  virtual void execute() override;
  virtual void threadJoin(const UserObject & y) override;

protected:
  /// The pseudo-density variable
  MooseWritableVariable * _design_density;
  /// The filtered density variable
  MooseWritableVariable * _filtered_density;
  /// The physical density variable
  MooseWritableVariable * _physical_density;

private:
  struct ElementData
  {
    Real design_density;
    Real filtered_density;
    Real projected_density;
    ElementData() = default;
    ElementData(Real dens, Real filt_dens, Real proj_dens)
      : design_density(dens), filtered_density(filt_dens), projected_density(proj_dens)
    {
    }
  };

  /// Data structure to hold old density, sensitivity, volume, current density.
  std::map<dof_id_type, ElementData> _elem_data_map;

  /**
   * Gathers element date necessary to perform the bisection algorithm for optimization
   */
  void gatherElementData();

  void densityFilter();

  void heavisideProjection();
};
