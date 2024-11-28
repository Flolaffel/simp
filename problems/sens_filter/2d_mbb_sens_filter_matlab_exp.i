nx = 40
ny = 20
w = 40
h = 20
p = 3
vol_frac = 0.3
filter_radius = 1.5
E0 = 1
Emin = 1e-9
nu = 0.3

[GlobalParams]
  displacements = 'disp_x disp_y'
  design_density = rho
  physical_density = rhoPhys
  volume_fraction = ${vol_frac}
  radius = ${filter_radius}
[]

[Mesh]
  [MeshGenerator]
    type = GeneratedMeshGenerator
    dim = 2
    nx = ${nx}
    ny = ${ny}
    xmin = '${fparse -w/2}'
    xmax = '${fparse w/2}'
    ymin = '${fparse -h/2}'
    ymax = '${fparse h/2}'
  []
  [node]
    type = ExtraNodesetGenerator
    input = MeshGenerator
    new_boundary = pull
    coord = '${fparse w/2} ${fparse -h/2} 0'
  []
  [push]
    type = ExtraNodesetGenerator
    input = node
    new_boundary = push
    coord = '${fparse -w/2} ${fparse h/2} 0'
  []
  [top_right]
    type = ExtraNodesetGenerator
    input = push
    new_boundary = tr
    coord = '${fparse w/2} ${fparse h/2} 0'
  []
  [bottom_right]
    type = ExtraNodesetGenerator
    input = top_right
    new_boundary = br
    coord = '${fparse w/2} ${fparse -h/2} 0'
  []
  [top_left]
    type = ExtraNodesetGenerator
    input = bottom_right
    new_boundary = tl
    coord = '${fparse -w/2} ${fparse h/2} 0'
  []
  [bottom_left]
    type = ExtraNodesetGenerator
    input = top_left
    new_boundary = bl
    coord = '${fparse -w/2} ${fparse -h/2} 0'
  []
  [sidesets]
    type = SideSetsFromNodeSetsGenerator
    input = bottom_left
  []
[]

[AuxVariables]
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
  [dc]
    family = MONOMIAL
    order = CONSTANT
  []
  [dc_AD_half]
    family = MONOMIAL
    order = CONSTANT
  []
  [dc_AD]
    family = MONOMIAL
    order = CONSTANT
  []
  [ct]
    family = SCALAR
    order = FIRST
  []
  [dct]
    family = MONOMIAL
    order = CONSTANT
  []
  [dV]
    family = MONOMIAL
    order = CONSTANT
  []
  [rho]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${vol_frac}
  []
  [rhoPhys]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${vol_frac}
  []
[]

[AuxKernels]
  [copy_compliance_sens]
    type = MaterialRealAux
    property = sensitivity
    variable = dc
    execute_on = TIMESTEP_END
  []
  [copy_AD_compliance_sens]
    type = MaterialRealAux
    property = AD_sensitivity
    variable = dc_AD_half
    execute_on = TIMESTEP_END
  []
  [dc_AD_parsed]
    type = ParsedAux
    variable = dc_AD
    coupled_variables = 'dc_AD_half'
    expression = '2*dc_AD_half'
  []
  [volume_sens]
    type = ConstantAux
    variable = dV
    value = 1
    execute_on = TIMESTEP_END
  []
[]

[Modules/TensorMechanics/Master]
  [all]
    strain = SMALL
    add_variables = true
    incremental = false
    generate_output = "vonmises_stress"
  []
[]

# [ICs]
#   [rho_IC]
#     type = BoundingBoxIC
#     variable = rho
#     x1 = -0.75
#     x2 = 0.751
#     y1 = -0.75
#     y2 = 0.751
#     inside = 1e-9
#     outside = ${vol_frac}
#     # inside = 1
#     # outside = 0.1
#   []
#   [rho_phys_IC]
#     type = BoundingBoxIC
#     variable = rhoPhys
#     x1 = -0.75
#     x2 = 0.751
#     y1 = -0.75
#     y2 = 0.751
#     inside = 1e-9
#     outside = ${vol_frac}
#     # inside = 1
#     # outside = 0.1
#   []
# []

# [NodalKernels]
#   [force]
#     type = ConstantRate
#     variable = disp_x
#     boundary = 'tl'
#     rate = 1
#   []
# []

# [BCs]
#   [no_y]
#     type = DirichletBC
#     variable = disp_y
#     boundary = 'bl br tl'
#     value = 0
#   []
#   [no_x]
#     type = DirichletBC
#     variable = disp_x
#     boundary = 'bl br'
#     value = 0
#   []
# []

[BCs]
  [no_x]
    type = DirichletBC
    variable = disp_y
    boundary = pull
    value = 0.0
  []
  [no_y]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0.0
  []
[]

[NodalKernels]
  [pull]
    type = ConstantRate
    variable = disp_y
    boundary = push
    rate = -1
  []
[]

[Materials]
  [elasticity_tensor]
    type = ComputeVariableIsotropicElasticityTensorPlaneStress
    youngs_modulus = E_phys
    poissons_ratio = poissons_ratio
    args = 'Emin rhoPhys p E0'
  []
  [E_phys]
    type = DerivativeParsedMaterial
    # Emin + (density^penal) * (E0 - Emin)
    expression = '${Emin} + (rhoPhys ^ ${p}) * (${E0}-${Emin})'
    coupled_variables = 'rhoPhys'
    property_name = E_phys
  []
  [poissons_ratio]
    type = GenericConstantMaterial
    prop_names = poissons_ratio
    prop_values = ${nu}
  []
  [stress]
    type = ComputeLinearElasticStress
  []
  [dc]
    type = AnalyticComplianceSensitivity
    physical_density = rhoPhys
    youngs_modulus = E_phys
    incremental = false
    E0 = ${E0}
    Emin = ${Emin}
    p = ${p}
  []
  [AD_elasticity_tensor]
    type = ComputeVariableIsotropicElasticityTensorPlaneStress
    youngs_modulus = E_phys
    poissons_ratio = poissons_ratio
    args = 'Emin rhoPhys p E0'
    base_name = AD
  []
  [AD_stress]
    type = ComputeLinearElasticStress
    base_name = AD
  []
  [AD_strain]
    type = ComputeSmallStrain
    base_name = AD
  []
  [dc_AD]
    type = ComplianceSensitivity
    design_density = rhoPhys
    youngs_modulus = E_phys
    incremental = false
    base_name = AD
  []
[]

[Preconditioning]
  active = 'superlu'
  [superlu]
    type = SMP
    full = true
    petsc_options = '-snes_ksp_ew'
    petsc_options_iname = '-pc_type -pc_factor_mat_solver_package -pc_factor_shift_type -pc_factor_shift_amount'
    petsc_options_value = 'lu       superlu_dist                  NONZERO               1e-15'
  []
  [print]
    type = SMP
    full = true
    petsc_options = '-snes_test_jacobian -snes_test_jacobian_view'
    petsc_options_iname = '-pc_type -pc_factor_mat_solver_package -pc_factor_shift_type -pc_factor_shift_amount'
    petsc_options_value = 'lu       superlu_dist                  NONZERO               1e-15'
  []
[]

[UserObjects]
  [update]
    type = DensityUpdateOC
    objective_function_sensitivity = dc
    volume_sensitivity = dV
    mesh_generator = MeshGenerator
  []
  # needs MaterialRealAux to copy sensitivity (mat prop) to dc aux variable
  # [calc_sense]
  #   type = SensitivityFilterCustom
  #   sensitivities = dc
  #   mesh_generator = MeshGenerator
  #   filter_type = sensitivity
  # []
  [compliance_sens]
    type = ComplianceResponse
    usage = objective
    value = ct
    sensitivity = dct
    physical_density = rhoPhys
    E0 = ${E0}
    Emin = ${Emin}
    p = ${p}
    poissons_ratio = ${nu}
    mesh_generator = MeshGenerator
  []
[]

[Postprocessors]
  [c]
    type = ElementIntegralMaterialProperty
    mat_prop = strain_energy_density
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON

  nl_abs_tol = 1e-6
  l_abs_tol = 1e-6
  dt = 1.0
  num_steps = 1

  abort_on_solve_fail = true
[]

[Outputs]
  exodus = true
[]

# [Debug]
#   check_jacobian = true
# []
