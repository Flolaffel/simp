nx = 150
ny = 75
xmax = 100
ymax = 50
l_el = '${fparse xmax/nx}'
p = 3
filter_radius = 2.5
E0 = 210000
Emin = 1e-9
nu = 0.3
start_dens = 0.5

[Problem]
  extra_tag_matrices = K
[]

[GlobalParams]
  displacements = 'disp_x disp_y'
  design_density = rho
  physical_density = rhoPhys
  radius = ${filter_radius}
  E0 = ${E0}
  Emin = ${Emin}
  p = ${p}
[]

[Mesh]
  [MeshGenerator]
    type = GeneratedMeshGenerator
    dim = 2
    nx = ${nx}
    ny = ${ny}
    xmin = 0
    xmax = ${xmax}
    ymin = 0
    ymax = ${ymax}
  []
  [node]
    type = ExtraNodesetGenerator
    input = MeshGenerator
    new_boundary = pull
    coord = '${fparse xmax} 0 0; ${fparse xmax-l_el} 0 0; ${fparse xmax-2*l_el} 0 0; ${fparse xmax-3*l_el} 0 0; ${fparse xmax-4*l_el} 0 0; ${fparse xmax-5*l_el} 0 0'
  []
  [push]
    type = ExtraNodesetGenerator
    input = node
    new_boundary = push
    coord = '0 50 0'
  []
  [sidesets]
    type = SideSetsFromNodeSetsGenerator
    input = push
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
  [V]
    family = SCALAR
    order = FIRST
  []
  [dV]
    family = MONOMIAL
    order = CONSTANT
  []
  [PN]
    family = SCALAR
    order = FIRST
  []
  [dPN]
    family = MONOMIAL
    order = CONSTANT
  []
  [rho]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${start_dens}
  []
  [rhoPhys]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${start_dens}
  []
  [rho_old1]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${start_dens}
  []
  [rho_old2]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${start_dens}
  []
  [low]
    family = MONOMIAL
    order = CONSTANT
  []
  [upp]
    family = MONOMIAL
    order = CONSTANT
  []
[]

[AuxKernels]
  [copy_compliance_sens]
    type = MaterialRealAux
    property = sensitivity
    variable = dc
    execute_on = TIMESTEP_END
  []
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
[]

[Kernels]
  [stress_x]
    type = StressDivergenceTensors
    component = 0
    variable = disp_x
    extra_matrix_tags = K
  []
  [stress_y]
    type = StressDivergenceTensors
    component = 1
    variable = disp_y
    extra_matrix_tags = K
  []
[]

[BCs]
  [no_x]
    type = DirichletBC
    variable = disp_y
    boundary = left
    value = 0.0
    matrix_tags = system
  []
  [no_y]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0.0
    matrix_tags = system
  []
  [pull]
    type = NeumannBC
    variable = disp_y
    boundary = pull
    value = -20
    matrix_tags = system
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
  [compute_strain]
    type = ComputeSmallStrain
  []
  [dc]
    type = AnalyticComplianceSensitivity
    physical_density = rhoPhys
    youngs_modulus = E_phys
    incremental = false
  []
  [elasticity_tensor_0]
    type = ComputeIsotropicElasticityTensor
    base_name = micro
    youngs_modulus = ${E0}
    poissons_ratio = ${nu}
  []
  [stress_micro]
    type = ComputeLinearElasticStress
    base_name = micro
  []
  [strain_micro]
    type = ComputeSmallStrain
    base_name = micro
  []
[]

[AuxVariables]
  [vonmises]
    order = CONSTANT
    family = MONOMIAL
  []
  [micro_stress_xx]
    order = CONSTANT
    family = MONOMIAL
  []
  [micro_stress_yy]
    order = CONSTANT
    family = MONOMIAL
  []
  [micro_stress_xy]
    order = CONSTANT
    family = MONOMIAL
  []
  [micro_vonmises_stress]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[AuxKernels]
  [vonmises]
    type = RankTwoScalarAux
    rank_two_tensor = stress
    variable = vonmises
    scalar_type = VonMisesStress
  []
  [micro_stress_xx]
    type = RankTwoAux
    rank_two_tensor = micro_stress
    variable = micro_stress_xx
    index_i = 0
    index_j = 0
  []
  [micro_stress_yy]
    type = RankTwoAux
    rank_two_tensor = micro_stress
    variable = micro_stress_yy
    index_i = 1
    index_j = 1
  []
  [micro_stress_xy]
    type = RankTwoAux
    rank_two_tensor = micro_stress
    variable = micro_stress_xy
    index_i = 0
    index_j = 1
  []
  [micro_vonmises_stress]
    type = RankTwoScalarAux
    rank_two_tensor = micro_stress
    variable = micro_vonmises_stress
    scalar_type = VonMisesStress
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[UserObjects]
  [update]
    type = DensityUpdateMMA
    objective_function_sensitivity = dV
    constraint_values = 'PN'
    constraint_sensitivities = 'dPN'
    old_design_density1 = rho_old1
    old_design_density2 = rho_old2
    mma_lower_asymptotes = low
    mma_upper_asymptotes = upp
    move_limit = 0.15
  []
  # needs MaterialRealAux to copy sensitivity (mat prop) to Dc aux variable
  [filt_sens]
    type = SensitivityFilterCustom
    filter_type = density
    sensitivities = 'dc dV dPN'
    mesh_generator = MeshGenerator
  []
  [filt_dens]
    type = DensityFilter
    design_density = rho
    physical_density = rhoPhys
    radius = ${filter_radius}
    mesh_generator = MeshGenerator
  []
  [stress_sens]
    type = StressResponseEpsPNorm
    usage = constraint
    limit = 100
    value = PN
    sensitivity = dPN
    stresses = 'micro_vonmises_stress micro_stress_xx micro_stress_xy micro_stress_yy'
    poissons_ratio = ${nu}
    mesh_generator = MeshGenerator
    system_matrix = K
  []
  [vol_sens]
    type = VolumeResponse
    usage = objective
    value = V
    sensitivity = dV
  []
  [opt_conv]
    type = Terminator
    expression = 'change < 0.005 & change != 0'
    execute_on = TIMESTEP_END
    execution_order_group = 5
  []
[]

[Postprocessors]
  [change]
    type = VectorPostprocessorComponent
    vectorpostprocessor = max_abs_diff
    vector_name = Difference
    index = 0
    execute_on = 'initial timestep_end'
  []
[]

[VectorPostprocessors]
  [max_abs_diff]
    type = ElementVariablesDifferenceMax
    compare_a = rho
    compare_b = rho_old1
    furthest_from_zero = true
    contains_complete_history = true
    execution_order_group = 10
  []
[]

[Executioner]
  type = Transient
  solve_type = Newton
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_package'
  petsc_options_value = 'lu superlu_dist'
  nl_abs_tol = 1e-8
  dt = 1
  num_steps = 2000
[]

[Outputs]
  exodus = true
  # [pgraph]
  #   type = PerfGraphOutput
  #   execute_on = 'initial final'  # Default is "final"
  #   level = 3                     # Default is 1
  #   heaviest_branch = true        # Default is false
  #   heaviest_sections = 7         # Default is 0
  # []
[]

[Debug]
  # show_material_props = true
  # show_execution_order = ALWAYS
[]
