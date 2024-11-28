import itertools
import netCDF4 as ex
import csv
import numpy as np
import os

# open file
file = ex.Dataset(
    "problems/shear_PBC_IC/simple/min_S/simple_shear_PBC_u_minS_50x50_C_out.e"
)

# get names of element variables
encodedScalarVarNames = file.variables["name_glo_var"]
scalarVarNames = []
for encName in encodedScalarVarNames:
    name = ""
    for char in encName:
        if not isinstance(char, np.ma.core.MaskedConstant):
            name += char.decode("UTF-8")
    scalarVarNames.append(name)

# get values of element variables
scalarVarIndex = []
scalarVars = [["t"] + scalarVarNames]
for i, timestep in enumerate(file.variables["vals_glo_var"]):
    scalarVars.append([i] + timestep.tolist())

file.close()

script_path = os.path.dirname(os.path.realpath(__file__))
with open(script_path + "/extract_scalar.csv", "w") as csvfile:
    writer = csv.writer(csvfile)
    writer.writerows(scalarVars)
