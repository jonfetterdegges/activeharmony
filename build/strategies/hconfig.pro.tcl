#
# Copyright 2003-2011 Jeffrey K. Hollingsworth
#
# This file is part of Active Harmony.
#
# Active Harmony is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Active Harmony is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with Active Harmony.  If not, see <http://www.gnu.org/licenses/>.
#

# Store the path to the base directory
global harmony_root
if { [string length harmony_root] == 0 } {
    error "$$harmony_root must be set prior to evaluating this file"
}

global search_algorithm
set search_algorithm 1 

puts "the backend files."
source ${harmony_root}/libexec/tcl/pro/utilities.tcl
source ${harmony_root}/libexec/tcl/pro/parseApp_version_8.tcl
source ${harmony_root}/libexec/tcl/pro/proparseApp_version_8.tcl
source ${harmony_root}/libexec/tcl/pro/pro.tcl
source ${harmony_root}/libexec/tcl/pro/initial_simplex_construction.tcl
source ${harmony_root}/libexec/tcl/pro/random_uniform.tcl
source ${harmony_root}/libexec/tcl/pro/matrix.tcl
source ${harmony_root}/libexec/tcl/pro/pro_transformations.tcl
source ${harmony_root}/libexec/tcl/pro/projection.tcl
source ${harmony_root}/libexec/tcl/pro/combine.tcl
source ${harmony_root}/libexec/tcl/pro/pro_init.tcl

puts "done loading the tcl related files"

