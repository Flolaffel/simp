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
 * Element user object that filters the objective function sensitivities via a radial average user
 * objects. This object can be used to apply a Solid Isotropic Material Penalization (SIMP) to
 * optimization.
 */
class SensitivityUpdateDensityFilter : public ElementUserObject
{
public:
  static InputParameters validParams();

  SensitivityUpdateDensityFilter(const InputParameters & parameters);

  virtual void initialize() override;
  virtual void execute() override;
  virtual void finalize() override{};
  virtual void threadJoin(const UserObject &) override{};

protected:
  /// The system mesh
  const MooseMesh & _mesh;
  /// Name of the mesh generator to get MeshMetaData from
  const MeshGeneratorName _mesh_generator;
  /// Sensitivity with respect to density
  MooseWritableVariable * _compliance_sensitivity;
  /// Sensitivity with respect to density
  MooseWritableVariable * _volume_sensitivity;
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
    Real compliance_sensitivity;
    Real volume_sensitivity;
    Real new_compliance_sensitivity;
    Real new_volume_sensitivity;
    ElementData() = default;
    ElementData(Real dc, Real dv, Real filt_dc, Real filt_dv)
      : compliance_sensitivity(dc),
        volume_sensitivity(dv),
        new_compliance_sensitivity(filt_dc),
        new_volume_sensitivity(filt_dv)
    {
    }
  };

  /// Data structure to hold old density, sensitivity, volume, current density.
  std::map<dof_id_type, ElementData> _elem_data_map;

  /// Data structures to hold the filter weights
  std::vector<std::vector<Real>> _H;
  std::vector<Real> _Hs;

  /**
   * Gathers element date necessary to perform the bisection algorithm for optimization
   */
  void gatherElementData();

  /**
   * Prepares data needed for filtering of new densitys
   */
  void prepareFilter();

  /**
   * Filters the compliance and volume sensitivities in respect to the density
   */
  void filterSensitivities();
};
