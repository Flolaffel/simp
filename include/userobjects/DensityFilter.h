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
 * Element user object that filters the design density.
 */
class DensityFilter : public FilterBase
{
public:
  static InputParameters validParams();

  DensityFilter(const InputParameters & parameters);

  virtual void initialize() override;
  virtual void execute() override;
  virtual void finalize() override{};
  virtual void threadJoin(const UserObject & y) override;

protected:
  /// The design density variable
  MooseWritableVariable * _design_density;
  /// The physical density variable
  MooseWritableVariable * _physical_density;

private:
  /// Data structure to hold current design density
  std::vector<std::pair<dof_id_type, Real>> _design_density_vec;

  /// Data structure to hold filtered (physical) density
  std::map<dof_id_type, Real> _filtered_density_map;

  /**
   * Gathers element data
   */
  void gatherElementData();

  /**
   * Filters the density
   */
  void densityFilter();
};
