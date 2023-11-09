nx = 50
ny = 50
p = 3
vol_frac = 0.5
filter_radius = 1.2
E0 = 210000
Emin = 0.001

[Mesh]
  [planet]
    type = GeneratedMeshGenerator
    dim = 2
    nx = ${nx}
    ny = ${ny}
  []
[]

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[AuxVariables]
  [rho]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${vol_frac}
  []
  [Dc]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = -1.0
  []
  [Emin]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${Emin}
  []
  [p]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${p}
  []
  [E0]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${E0}
  []
[]

[Modules/TensorMechanics/Master]
  [all]
    strain = SMALL
    add_variables = true
    incremental = false
    generate_output = 'vonmises_stress'
  []
[]

[Functions]
  [tforce]
    type = ParsedFunction
    expression = 't'
  []
[]

[BCs]
  [disp_y]
    type = DirichletBC
    variable = disp_y
    boundary = 'bottom top'
    value = 0
  []
  [disp_x]
    type = DirichletBC
    variable = disp_x
    boundary = 'bottom'
    value = 0
  []
  [tdisp]
    type = NeumannBC
    variable = disp_x
    boundary = top
    value = 1
  []
[]

[Materials]
  [elasticity_tensor]
    type = ComputeVariableIsotropicElasticityTensor
    youngs_modulus = E_phys
    poissons_ratio = 0.3
    args = 'Emin rho p E0'
  []
  [E_phys]
    type = DerivativeParsedMaterial
    # Emin + (density^penal) * (E0 - Emin)
    expression = '${Emin} + (rho ^ ${p}) * (${E0}-${Emin})'
    coupled_variables = 'rho'
    property_name = E_phys
  []
  [dc]
    type = ComplianceSensitivity
    design_density = rho
    youngs_modulus = E_phys
  []
  [compute_stress]
    type = ComputeLinearElasticStress
  []
[]

[UserObjects]
  [update]
    type = DensityUpdate
    density_sensitivity = Dc
    design_density = rho
    volume_fraction = ${vol_frac}
    execute_on = TIMESTEP_BEGIN
  []
  [rad_avg]
    type = RadialAverage
    radius = ${filter_radius}
    weights = linear
    prop_name = sensitivity
    execute_on = TIMESTEP_END
    force_preaux = true
  []
  [calc_sense]
    type = SensitivityFilter
    density_sensitivity = Dc
    design_density = rho
    filter_UO = rad_avg
    execute_on = TIMESTEP_END
    force_postaux = true
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_package'
  petsc_options_value = 'lu superlu_dist'
  dt = 1.0
  num_steps = 500
  nl_abs_tol = 1e-8
[]

[Outputs]
  exodus = true
[]

[Postprocessors]
  [total_vol]
    type = ElementIntegralVariablePostprocessor
    variable = rho
    execute_on = 'INITIAL TIMESTEP_END'
  []
[]
