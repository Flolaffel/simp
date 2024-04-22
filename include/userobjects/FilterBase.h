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
#include "MooseEnum.h"

/**
 * Element user object that provides basic filter functionality for derived classes
 */

enum class FilterType
{
  NONE,
  SENSITIVITY,
  DENSITY,
  HEAVISIDE
};

class FilterBase : public ElementUserObject
{
public:
  static InputParameters validParams();

  FilterBase(const InputParameters & parameters);

  virtual void initialSetup() override;
  virtual void initialize() override{};
  virtual void execute() override{};
  virtual void finalize() override;
  virtual void threadJoin(const UserObject &) override{};

  /**
   * Get filter type
   * @return enum
   */
  static MooseEnum getFilterEnum();

  /**
   * Define parameters used by multiple filter objects
   * @return InputParameters object populated with common parameters
   */
  static InputParameters commonParameters();

protected:
  /// Filter type
  const FilterType _filter_type;
  /// The system mesh
  const MooseMesh & _mesh;
  /// Name of the mesh generator to get MeshMetaData from
  MeshGeneratorName _mesh_generator;
  /// Cut-off radius for filtering
  Real _radius;
  /// Number of elements in X direction
  unsigned int _nx;
  /// Number of elements in Y direction
  unsigned int _ny;
  /// Total number of elements
  unsigned int _n_el;
  /// Lower X Coordinate of the generated mesh
  Real _xmin;
  /// Upper X Coordinate of the generated mesh
  Real _xmax;
  /// Lower Y Coordinate of the generated mesh
  Real _ymin;
  /// Upper Y Coordinate of the generated mesh
  Real _ymax;

  /// Data structures to hold the filter weights
  std::vector<std::vector<Real>> _H;
  std::vector<Real> _Hs;

  /**
   * Prepares data needed for filtering
   */
  void prepareFilter();

  /// Heaviside parameters
  Real _beta_0;
  Real _beta_max;
  Real _beta;
  Real _eta;

private:
};
