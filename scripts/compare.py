import itertools
import netCDF4 as ex
import csv
import numpy as np

# open files
file1 = ex.Dataset(
    "problems/simple_shear_PBC_IC/mpi/simple_shear_2x2_moose_min_tol_out_serial.e"
)
file2 = ex.Dataset(
    "problems/simple_shear_PBC_IC/mpi/simple_shear_2x2_moose_min_tol_out_mpi.e"
)

# get names of nodal variables
encodedNodalVarNames = file1.variables["name_nod_var"]
nodalVarNames = []
for encName in encodedNodalVarNames:
    name = ""
    for char in encName:
        if not isinstance(char, np.ma.core.MaskedConstant):
            name += char.decode("UTF-8")
    nodalVarNames.append(name)

# get names of element variables
encodedElementVarNames = file1.variables["name_elem_var"]
elementVarNames = []
for encName in encodedElementVarNames:
    name = ""
    for char in encName:
        if not isinstance(char, np.ma.core.MaskedConstant):
            name += char.decode("UTF-8")
    elementVarNames.append(name)

# get values of nodal variables
checkNodalVars = ["disp_x", "disp_y"]
nodalVarIndex = []
nodalVars1 = []
nodalVars2 = []
for var in checkNodalVars:
    index = nodalVarNames.index(var) + 1
    # get values of desired variable
    timeStep = 1
    nodalVars1.append(file1.variables["vals_nod_var" + str(index)][timeStep])
    nodalVars2.append(file2.variables["vals_nod_var" + str(index)][timeStep])

# get values of element variables

# checkElemVars = [
#     "rho",
#     "dc",
#     "stress_xx",
#     "stress_yy",
#     "stress_xy",
#     "strain_xx",
#     "strain_yy",
#     "strain_xy",
# ]

# checkElemVars = [
#     "rho",
#     "dc",
#     "cauchy_stress_xx",
#     "cauchy_stress_yy",
#     "cauchy_stress_xy",
#     "strain_xx",
#     "strain_yy",
#     "strain_xy",
# ]
# elemVarIndex = []
# elemVars1 = []
# elemVars2 = []
# for var in checkElemVars:
#     index = elementVarNames.index(var) + 1
#     # get values of desired variable
#     timeStep = 1
#     elemVars1.append(file1.variables["vals_elem_var" + str(index) + "eb1"][timeStep])
#     elemVars2.append(file2.variables["vals_elem_var" + str(index) + "eb1"][timeStep])

file1.close()
file2.close()

# if len(elemVars1) != len(elemVars2):
#     raise Exception("Arrays have to be of same length")

print(nodalVars1[0][1])
print(nodalVars2[0][1])

# print(elemVars1[5][0])
# print(elemVars2[5][0])

# absDiff = 0
# for i in range(len(elemVars1)):
#     absDiff += abs(elemVars1[i] - elemVars2[i])
# print(absDiff)
