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

enum class Usage
{
  OBJECTIVE,
  CONSTRAINT
};

enum class ConstraintType
{
  ABSOLUTE,
  RELATIVE
};

enum class InequalityType
{
  LE,
  GE
};

class TODesignResponse : public ElementUserObject
{
public:
  static InputParameters validParams();

  TODesignResponse(const InputParameters & parameters);

  virtual void initialize() override {};
  virtual void execute() override {};
  virtual void finalize() override {};
  virtual void threadJoin(const UserObject &) override {};

  /**
   * Gathers element data necessary to calculate stress sensitivity
   */
  virtual void gatherElementData() = 0;

  /**
   * Adaptively scales constraint
   */
  virtual void scaleConstraint() = 0;

  /**
   * Calculates the value
   */
  virtual void computeValue() = 0;

  /**
   * Calculates the sensitivity
   */
  virtual void computeSensitivity() = 0;

protected:
  /// Number of elements
  unsigned int _n_el;
  /// The usage
  const Usage _usage;
  /// Constraint type
  const InequalityType _inequality_type;
  /// Flag for objective function
  const bool _is_objective;
  /// Flag for constraint
  const bool _is_constraint;
  /// Limit
  Real _limit;
  /// Scaling
  bool _scaling;
  /// scaled limit
  Real _scaled_limit;
  /// Value variable
  MooseVariableScalar * _scalar_value;
  /// Value
  Real _value;
  /// Sensitivity with respect to density
  MooseWritableVariable * _sensitivity;
  /// Physical density variable name
  const VariableName _physical_density_name;
  /// Physical density variable
  const MooseVariable * _physical_density;
  // TODO: Already collect design density here and supply to all responses?
  /// Sign to differentiate less equal/greater equal
  int _con_sign;

  /**
   * Get usage
   * @return enum
   */
  static MooseEnum getUsageEnum();

  /**
   * Get inequality type
   * @return enum
   */
  static MooseEnum getInequalityTypeEnum();
};
