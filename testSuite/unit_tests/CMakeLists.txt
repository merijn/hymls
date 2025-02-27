set(SOURCES
  GaleriExt_Darcy2D
  GaleriExt_Darcy3D
  GaleriExt_Stokes2D
  GaleriExt_Stokes3D
  HYMLS_AugmentedMatrix
  HYMLS_CartesianPartitioner
  HYMLS_SkewCartesianPartitioner
  HYMLS_DenseUtils
  HYMLS_HierarchicalMap
  HYMLS_OverlappingPartitioner
  HYMLS_Preconditioner
  HYMLS_ProjectedOperator
  HYMLS_CoarseSolver
  HYMLS_Solver
  HYMLS_BorderedSolver
  HYMLS_SparseDirectSolver
  HYMLS_Tester
  HYMLS_Tools
  HYMLS_UnitTests
  HYMLS_FakeComm
  main)

if (HAVE_TEUCHOS_COMPLEX)
  list(APPEND SOURCES
    HYMLS_ComplexOperator
    HYMLS_ComplexSolver
    )
endif()

# Configure the data xml file in which we can set parameters (like file names)
configure_file(data.xml ${CMAKE_CURRENT_BINARY_DIR}/data.xml)
configure_file(HYMLS_UnitTestData.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/HYMLS_UnitTestData.hpp)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(unit_tests ${SOURCES})

target_link_libraries(unit_tests hymls)

execute_process(COMMAND ${MPIEXEC} --oversubscribe echo RESULT_VARIABLE status OUTPUT_QUIET)
if (NOT status)
  set(MPI_OVERSUBSCRIBE "--oversubscribe")
endif()

add_test(NAME unit_tests_1 COMMAND ./unit_tests)
add_test(NAME unit_tests_2 COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 2 ${MPI_OVERSUBSCRIBE} ./unit_tests)
add_test(NAME unit_tests_3 COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 3 ${MPI_OVERSUBSCRIBE} ./unit_tests)
add_test(NAME unit_tests_4 COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 4 ${MPI_OVERSUBSCRIBE} ./unit_tests)
add_test(NAME unit_tests_5 COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 5 ${MPI_OVERSUBSCRIBE} ./unit_tests)
add_test(NAME unit_tests_8 COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 8 ${MPI_OVERSUBSCRIBE} ./unit_tests)

set(test_list unit_tests_1 unit_tests_2 unit_tests_3 unit_tests_4 unit_tests_5 unit_tests_8)
set_tests_properties(${test_list} PROPERTIES PASS_REGULAR_EXPRESSION "TEST PASSED")
set_tests_properties(${test_list} PROPERTIES FAIL_REGULAR_EXPRESSION "TEST FAILED")
set_tests_properties(${test_list} PROPERTIES TIMEOUT 500)
