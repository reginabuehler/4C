# This file is part of 4C multiphysics licensed under the
# GNU Lesser General Public License v3.0 or later.
#
# See the LICENSE.md file in the top-level for license information.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Import python modules.
import os
import sys
import numpy as np
import xml.etree.ElementTree as ET
from vtk import vtkXMLPPolyDataReader
from vtk import vtkXMLPUnstructuredGridReader
from vtk import vtkXMLGenericDataObjectReader
from vtk.util import numpy_support as VN

# Import pure vtk data comparison.
from vtk_data_compare import compare_vtk_data

# we expect the following input:
# link_to_python link_to_this_script input_pvd_file reference_pvd_file tolerance_for_data_comparison number_of_timesteps[optional] list_of_points_in_time[optional]
# /home/user/anaconda3/envs/vtk-test/bin/python /home/user/sim/vtk-tests/python/vtk_compare.py /home/user/sim/vtk-tests/sohex8/xxx-structure.pvd /home/user/sim/vtk-tests/sohex8/sohex8fbar_cooks_nl_new_struc-structure.pvd 1e-8 3 10.0 35.0 100.0


def compare_vtk(path1, path2, points_in_time, tol_float=1e-8, raise_error=True):
    """
    Compare the vtk files at path1 and path2.

    Args
    ----
    raise_error: bool
        If true, then an error will be raised in case the files do not match. User can read it in verbose ctest.
        Otherwise False will be returned.
    tol_float: float
        If given, numbers will be considered equal if the difference between
        them is smaller than tol_float.
    timesteps: float list
        Only given timesteps will be compared. If empty all timesteps are compared.
    """

    # Check that both arguments are paths and .pvd files exist.
    if not (os.path.isfile(path1) and os.path.isfile(path2)):
        raise ValueError("The .pvd paths given are not OK!")

    def compare_pvd(path_comp, path_ref):
        """
        check content of pvd files, exclude file links
        """
        comp_data = ET.parse(path_comp)
        ref_data = ET.parse(path_ref)

        comp_root = comp_data.getroot()
        ref_root = ref_data.getroot()

        num_collections = len(comp_root.findall("Collection"))
        num_collections_ref = len(ref_root.findall("Collection"))

        if num_collections != num_collections_ref:
            raise ValueError(
                f"Number of Collections in PVD file differ! {num_collections} != {num_collections_ref}"
            )

        for collection, collection_ref in zip(
            comp_root.findall("Collection"), ref_root.findall("Collection")
        ):
            num_datasets = len(collection.findall("DataSet"))
            num_datasets_ref = len(collection_ref.findall("DataSet"))
            if num_datasets != num_datasets_ref:
                raise ValueError(
                    f"Number of DataSets in Collections of PVD file differ! {num_datasets} != {num_datasets_ref}"
                )

        # remove file attrib to compare the rest
        for collection in comp_root.findall("Collection"):
            for dataset in collection.findall("DataSet"):
                dataset.attrib.pop("file")

        for collection in ref_root.findall("Collection"):
            for dataset in collection.findall("DataSet"):
                dataset.attrib.pop("file")

        # Check that both etrees are the same except from file links
        if not (ET.tostring(comp_root) == ET.tostring(ref_root)):
            raise ValueError(
                f"XML structures in PVD files differ!\n\n{ET.tostring(comp_root).decode()}\n\nvs.\n\n{ET.tostring(ref_root).decode()}"
            )

    def find_pvtk(pvdpath, points_in_time):
        """
        Find list of pvtk files in proximity of given timesteps
        """
        mydata = ET.parse(pvdpath)

        filearray = []

        # find relative paths to pvtk files
        for type_tag in mydata.findall("Collection/DataSet"):
            stepfile = type_tag.get("file")
            timepoint = float(type_tag.get("timestep"))
            # only use timestep if it is close to values in given list or all are used
            if (
                np.isclose(
                    timepoint,
                    points_in_time,
                    atol=1e-10,
                    rtol=0.0,
                ).any()
            ) or (len(points_in_time) == 0):
                filearray.append(stepfile)

        # if something did not go as intended
        if (len(points_in_time) != len(filearray)) and not (len(points_in_time) == 0):
            raise ValueError(
                "Number of time steps given does not match number of files found! Check input or adjust tolerance."
            )

        return filearray

    def compare_pvtk(pvtkpath_comp, pvtkpath_ref):
        """
        compare file content of pvtk files
        """
        # Check that both .pvtk files exist
        if not (os.path.isfile(pvtkpath_comp) and os.path.isfile(pvtkpath_ref)):
            raise ValueError("The .pvtk paths given are not OK!")

        comp_data = ET.parse(pvtkpath_comp)
        ref_data = ET.parse(pvtkpath_ref)

        comp_root = comp_data.getroot()
        ref_root = ref_data.getroot()

        # remove piece elements (links) to compare the rest
        for child in comp_root:
            for grandchild in child.findall("Piece"):
                child.remove(grandchild)

        for child in ref_root:
            for grandchild in child.findall("Piece"):
                child.remove(grandchild)

        # Check that both etrees are the same except from file links
        if not (ET.tostring(comp_root) == ET.tostring(ref_root)):
            raise ValueError("XML structures in PVTK files differ!")

    def merge_vtk(pvtkpath):
        """
        use suiting vtkXMLP* reader to read data and return output
        """

        # find all desired vtk files and check if they exist
        vtkfiles = find_vtk_and_check(pvtkpath)

        dir = os.path.dirname(pvtkpath)

        # examplarily read the first vtk file to get its type
        reader = vtkXMLGenericDataObjectReader()
        reader.SetFileName(os.path.join(dir, vtkfiles[0]))
        reader.Update()
        type = reader.GetOutput().GetDataObjectType()

        # use the respective reader for both .vtk types used in 4C
        if type == 4:
            preader = vtkXMLPUnstructuredGridReader()
            preader.SetFileName(pvtkpath)
            preader.Update()
        else:
            raise ValueError("Unknown VTK result type. known types: UnstructuredGrid")

        # A matching sorting could be introduced here to get rid of dependency on number of processors

        return preader.GetOutput()

    def find_vtk_and_check(pvtkpath):
        """
        get list of every vtk file from every processor
        """

        mydata = ET.parse(pvtkpath)

        filearray = []

        for type_attrib in mydata.findall("*/Piece"):
            procfile = type_attrib.get("Source")
            # check if file exists
            if not (os.path.isfile(os.path.join(os.path.dirname(pvtkpath), procfile))):
                raise ValueError("The .vtk paths given are not OK!")
            filearray.append(procfile)

        return filearray

    # Perform all checks, catch errors.
    try:
        # compare content of .pvd files excluding filepaths
        compare_pvd(path_comp=path1, path_ref=path2)

        dir1 = os.path.dirname(path1)
        dir2 = os.path.dirname(path2)

        # create list of .pvtk files from .pvd file content
        pvtkpaths1 = find_pvtk(pvdpath=path1, points_in_time=points_in_time)
        pvtkpaths2 = find_pvtk(pvdpath=path2, points_in_time=points_in_time)

        # Load the vtk files.
        data1array = []
        data2array = []

        # pvtkpaths representing timestep files
        for i in range(0, len(pvtkpaths1)):
            compare_pvtk(
                pvtkpath_comp=os.path.join(dir1, pvtkpaths1[i]),
                pvtkpath_ref=os.path.join(dir2, pvtkpaths2[i]),
            )

        for iter in enumerate(pvtkpaths1):
            data1array.append(merge_vtk(os.path.join(dir1, iter[1])))

        for iter in enumerate(pvtkpaths2):
            data2array.append(merge_vtk(os.path.join(dir2, iter[1])))

        for i in range(0, len(data1array)):
            compare_vtk_data(data1array[i], data2array[i], tol_float=tol_float)

    except Exception as error:
        if raise_error:
            raise error
        return False

    return True


if __name__ == "__main__":
    # Read arguments.
    file_comp = sys.argv[1]
    file_ref = sys.argv[2]
    try:
        tolerance = float(sys.argv[3])
    except ValueError:
        print("Given tolerance is no float! Check your arguments!")
    tolerance = float(sys.argv[3])

    points_in_time = []
    # if timesteps are not given and variable remains empty, all timesteps are used

    if len(sys.argv) > 4:
        num_timesteps = int(sys.argv[4])
        if len(sys.argv) - 5 != num_timesteps:
            raise ValueError(
                "You did not list as many timesteps as you specified! Check your arguments!"
            )
        # if 0 is given as number of files this should still work
        if num_timesteps != 0:
            points_in_time = np.array(sys.argv[5 : 5 + num_timesteps], dtype=float)
    compare_vtk(
        path1=file_comp,
        path2=file_ref,
        points_in_time=points_in_time,
        tol_float=tolerance,
    )

    print(
        "SUCCESS: VTK results match for given .pvd files, points in time and tolerance"
    )
