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

/**
 * Element user object that copmutes the absolute value and sensitivity for a volume response in
 * Solid Isotropic Material Penalization (SIMP) optimization.
 */
class VolumeResponse : public TODesignResponse
{
public:
  static InputParameters validParams();

  VolumeResponse(const InputParameters & parameters);

  virtual void initialize() override;
  virtual void execute() override;
  virtual void finalize() override {};
  virtual void threadJoin(const UserObject &) override;

  virtual void gatherElementData() override;

  /**
   * Computes g = sum(x)/V_lim - 1
   */
  virtual void computeValue() override;

  /**
   * Computes dg/dx = 1/V_lim
   */
  virtual void computeSensitivity() override;

private:
  struct ElementData
  {
    Real physical_density;
    Real new_volume_sensitivity;
    ElementData() = default;
    ElementData(Real dens, Real new_dv) : physical_density(dens), new_volume_sensitivity(new_dv) {}
  };

  /// Data structure to hold elemental values
  std::map<dof_id_type, ElementData> _elem_data_map;
};
