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
#include "MooseTypes.h"

/**
 * Base class for topology optimization design responses
 */
class TopologyOptimizationDesignResponse : public ElementUserObject
{
public:
  static InputParameters validParams();

  TopologyOptimizationDesignResponse(const InputParameters & parameters);

  virtual void initialize() override{};
  virtual void execute() override{};
  virtual void finalize() override{};
  virtual void threadJoin(const UserObject &) override{};

  /**
   * Gathers element data necessary to calculate stress sensitivity
   */
  virtual void gatherElementData() = 0;

  /**
   * Calculates the value
   */
  virtual void computeValue() = 0;

  /**
   * Calculates the sensitivity
   */
  virtual void computeSensitivity() = 0;

protected:
  /// Limit
  const Real _limit;
  /// Value
  MooseVariableScalar * _value;
  /// Sensitivity with respect to density
  MooseWritableVariable * _sensitivity;
  /// Design density variable name
  const VariableName _design_density_name;
  /// Design density variable
  const MooseVariable * _design_density;
  /// Physical density variable name
  const VariableName _physical_density_name;
  /// Physical density variable
  const MooseVariable * _physical_density;
  // TODO: Already collect design density here and supply to all responses?
};
