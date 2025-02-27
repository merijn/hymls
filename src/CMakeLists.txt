set(HYMLS_SOURCE HYMLS_Solver
  HYMLS_BaseSolver
  HYMLS_DeflatedSolver
  HYMLS_BorderedSolver
  HYMLS_BorderedDeflatedSolver
  HYMLS_BorderedOperator
  HYMLS_BorderedVector
  HYMLS_Preconditioner
  HYMLS_SchurComplement
  HYMLS_SchurPreconditioner
  HYMLS_SeparatorGroup
  HYMLS_OverlappingPartitioner
  HYMLS_HierarchicalMap
  HYMLS_BasePartitioner
  HYMLS_CartesianPartitioner
  HYMLS_SkewCartesianPartitioner
  HYMLS_HyperCube
  HYMLS_MatrixUtils
  HYMLS_DenseUtils
  HYMLS_ProjectedOperator
  HYMLS_Epetra_Time
  HYMLS_EpetraExt_ProductOperator
  HYMLS_SparseDirectSolver
  HYMLS_CoarseSolver
  HYMLS_Householder
  HYMLS_AugmentedMatrix
  HYMLS_Tools
  HYMLS_Tester
  HYMLS_PLA
  HYMLS_Exception
  HYMLS_MatrixBlock
  HYMLS_ShiftedOperator
  HYMLS_MainUtils
  GaleriExt_CrsMatrices
  GaleriExt_Periodic
  EpetraExt_RestrictedCrsMatrixWrapper
  EpetraExt_RestrictedMultiVectorWrapper)

set(HEADERS)

# Manually add headers without source file
set(HEADERS ${HEADERS}
  GaleriExt_Cross2DN.h
  GaleriExt_Cross3DN.h
  GaleriExt_Darcy2D.h
  GaleriExt_Darcy3D.h
  GaleriExt_Star3D.h
  GaleriExt_Stokes2D.h
  GaleriExt_Stokes3D.h
  HYMLS_Macros.hpp
  HYMLS_no_debug.hpp
  HYMLS_OrthogonalTransform.hpp
  HYMLS_RestrictedOT.hpp
  )

if (HAVE_TEUCHOS_COMPLEX)
  list(APPEND HYMLS_SOURCE
    HYMLS_ComplexSolver
    HYMLS_ComplexBorderedSolver
    HYMLS_ComplexOperator
    HYMLS_ComplexVector
    )
endif()

if ("NOX" IN_LIST Trilinos_PACKAGE_LIST)
  list(APPEND HYMLS_SOURCE NOX_Epetra_LinearSystem_Hymls)
endif()

if (${phist_FOUND})
  message(STATUS "phist found in ${PHIST_INCLUDE_DIRS}")

  list(APPEND HYMLS_SOURCE HYMLS_PhistCustomCorrectionSolver)
  
  list(APPEND HEADERS
    HYMLS_PhistWrapper.hpp
    HYMLS_PhistCustomCorrectionSolver.hpp
    AnasaziPhistSolMgr.hpp)
endif()

# create header file to tell the source code about some config-time settings and features it can use
configure_file(${PROJECT_SOURCE_DIR}/src/HYMLS_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/HYMLS_config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
list(APPEND HEADERS "${CMAKE_CURRENT_BINARY_DIR}/HYMLS_config.h")

# Add header files belonging to cpp files. Headers include eachother,
#so for now just assume we want to install all header files that have a source.
set(header_extensions ".h" ".hpp")
foreach(i ${HYMLS_SOURCE})
  foreach(j ${header_extensions})
    if (EXISTS "${PROJECT_SOURCE_DIR}/src/${i}${j}")
      list(APPEND HEADERS "${i}${j}")
    endif()
  endforeach()
endforeach()
unset(header_extensions)

if (${HYMLS_MEMORY_PROFILING})
  list(APPEND HYMLS_SOURCE HYMLS_Malloc)
endif()

add_library(hymls ${HYMLS_SOURCE})

target_link_libraries(hymls ${Trilinos_LIBRARIES})
target_link_libraries(hymls ${Trilinos_TPL_LIBRARIES})
target_link_libraries(hymls ${MPI_CXX_LIBRARIES})
target_link_libraries(hymls ${OpenMP_CXX_LIBRARIES})

if (${phist_FOUND})
  target_include_directories(hymls PUBLIC ${PHIST_INCLUDE_DIRS})
  target_link_libraries(hymls ${PHIST_LIBRARIES})
endif()

if (${HYMLS_MEMORY_PROFILING})
  target_link_libraries(hymls ${CMAKE_DL_LIBS})
endif()

add_executable(hymls_main main.cpp)
target_link_libraries(hymls_main hymls)

add_executable(hymls_main_eigs main_eigs.cpp)
target_link_libraries(hymls_main_eigs hymls)

set(INCLUDE_INSTALL_DIR include)
set(LIB_INSTALL_DIR lib)
set(BIN_INSTALL_DIR bin)

# Install executables
install(TARGETS hymls_main EXPORT HYMLSTargets RUNTIME DESTINATION ${BIN_INSTALL_DIR})
install(TARGETS hymls_main_eigs EXPORT HYMLSTargets RUNTIME DESTINATION ${BIN_INSTALL_DIR})

# Install libraries
set(library_list)

if (BUILD_SHARED_LIBS)
  install(TARGETS hymls EXPORT HYMLSTargets LIBRARY DESTINATION ${LIB_INSTALL_DIR})
else()
  install(TARGETS hymls EXPORT HYMLSTargets ARCHIVE DESTINATION ${LIB_INSTALL_DIR})
endif()
list(APPEND library_list hymls)

# Install headers
install(FILES ${HEADERS} DESTINATION ${INCLUDE_INSTALL_DIR})

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/HYMLS/HYMLSConfigVersion.cmake"
  VERSION 0 COMPATIBILITY SameMajorVersion
  )

set(ConfigPackageLocation lib/cmake/HYMLS)
configure_package_config_file("${PROJECT_SOURCE_DIR}/cmake/HYMLSConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/HYMLSConfig.cmake" INSTALL_DESTINATION ${ConfigPackageLocation}
  PATH_VARS INCLUDE_INSTALL_DIR)

install(EXPORT HYMLSTargets FILE HYMLSTargets.cmake DESTINATION ${ConfigPackageLocation})
install(FILES "${PROJECT_BINARY_DIR}/HYMLSConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/HYMLS/HYMLSConfigVersion.cmake"
  DESTINATION ${ConfigPackageLocation}
  )
