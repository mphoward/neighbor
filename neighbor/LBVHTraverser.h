// Copyright (c) 2018, Michael P. Howard.
// This file is released under the Modified BSD License.

// Maintainer: mphoward

#ifndef NEIGHBOR_LBVH_TRAVERSER_H_
#define NEIGHBOR_LBVH_TRAVERSER_H_

#include "LBVH.h"
#include "LBVHTraverser.cuh"

#include "hoomd/ExecutionConfiguration.h"
#include "hoomd/GlobalArray.h"
#include "hoomd/GPUFlags.h"
#include "hoomd/Autotuner.h"

namespace neighbor
{

//! Linear bounding volume hierarchy traverser using stackless rope scheme.
/*!
 * A LBVHTraverser implements a scheme to traverse a LBVH. For example, two options
 * are using a stack-based traversal or using a stackless rope traversal scheme.
 * A LBVHTraverser will typically take the data from the LBVH and compress it into a
 * format that is efficient for traversal. During this step, the LBVHTraverser is also
 * permitted to modify the LBVH before compression, if it is useful for traversal (e.g.,
 * performing subtree collapse).
 *
 * The LBVH is traversed using a stackless scheme based on skip ropes. The
 * general idea is to store (for each node) the left child (to descend if an
 * overlap occurs) and a skip "rope" to the next node that should be processed
 * (if there is no overlap). The skip rope may point to the LBVHSentinel, which
 * indicates that traversal should be terminated.
 *
 * In order to efficiently perform this traversal, the LBVH data is heavily compressed
 * by the traverser. Each node is represented by an int4 (16B). This is much smaller than
 * the original LBVH data (~56B), and is achieved by compressing the bounding boxes into
 * a low-precision (10-bit) form. The LBVH root node is discretized into 2^10 bins. The
 * remaining bounding boxes are snapped onto this grid in a way that ensures correctness
 * (lower bounds are always rounded down, while upper bounds are always rounded up). The
 * (x,y,z) components are concatenated into one integer (4B) versus the original float3 (12B).
 * The nodes are decompressed into floats during traversal, again in a conservative way to
 * ensure the original node is always enclosed by the compressed/decompressed node. Some
 * additional overlaps can be generated by intersecting these nodes, but this is usually
 * a small number for typical simulations.
 *
 * The query spheres are permitted to take on Scalar precision. During traversal, the sphere
 * is translated (in Scalar precision) by the desired image. The sphere is then dropped into
 * single precision for traversal, but the sphere radius is rounded up to ensure that the
 * original volume is still enclosed. This again guarantees that overlaps that would be found
 * in Scalar precision are always found in the lower precision representation. There may be
 * some false positive overlaps, but these could be filtered or removed in a narrow-phase
 * collision if desired.
 *
 * The LBVH is not aware of periodic boundary conditions of a scene, and so by default the
 * LBVHTraverser only intersects the sphere directly against the LBVH. However, an additional
 * image list can be specified for ::traverse. The image list specifies *additional* translations
 * of the particle to consider, beyond the original sphere.
 */
class LBVHTraverser
    {
    public:
        //! Simple constructor for a LBVHTraverser.
        LBVHTraverser(std::shared_ptr<const ExecutionConfiguration> exec_conf);

        //! Destructor
        ~LBVHTraverser();

        //! Traverse the LBVH.
        template<class OutputOpT>
        void traverse(OutputOpT& out,
                      const GlobalArray<Scalar4>& spheres,
                      unsigned int N,
                      const LBVH& lbvh,
                      const GlobalArray<Scalar3>& images = GlobalArray<Scalar3>());

        //! Access the compressed LBVH data for traversal
        const GlobalArray<int4>& getData() const
            {
            return m_data;
            }

        //! Set the kernel autotuner parameters
        /*!
         * \param enable If true, run the autotuners. If false, disable them.
         * \param period Number of traversals between running the autotuners.
         */
        void setAutotunerParams(bool enable, unsigned int period)
            {
            m_tune_traverse->setEnabled(enable);
            m_tune_traverse->setPeriod(period);

            m_tune_compress->setEnabled(enable);
            m_tune_compress->setPeriod(period);
            }

    private:
        std::shared_ptr<const ExecutionConfiguration> m_exec_conf;  //!< Execution configuration

        GlobalArray<int4> m_data;        //!< Internal representation of the LBVH for traversal
        GPUFlags<float3> m_lbvh_lo;   //!< Lower bound of tree
        GPUFlags<float3> m_lbvh_hi;   //!< Upper bound of tree
        GPUFlags<float3> m_bins;      //!< Bin size for compression

        std::unique_ptr<Autotuner> m_tune_traverse; //!< Autotuner for traversal kernel
        std::unique_ptr<Autotuner> m_tune_compress; //!< Autotuner for compression kernel

        //! Compresses the lbvh into internal representation
        void compress(const LBVH& lbvh);
    };

        /*!
         * \param out Number of overlaps per sphere.
         * \param spheres Test spheres.
         * \param N Number of test spheres.
         * \param lbvh LBVH to traverse.
         * \param images Additional images of \a spheres to test.
         *
         * The format for a \a sphere is (x,y,z,R), where R is the radius of the sphere.
         */
/*!
 * \param out Output operation for intersected primitives.
 * \param spheres Test spheres.
 * \param N Number of test spheres.
 * \param lbvh LBVH to traverse.
 * \param images Additional images of \a spheres to test.
 *
 * \tparam OutputOpT The type of output operation.
 *
 * The format for a \a sphere is (x,y,z,R), where R is the radius of the sphere.
 *
 * A maximum of 32 \a images are allowed due to the internal representation of the image list
 * in the traversal CUDA kernel. This is more than enough to perform traversal in 3D periodic
 * boundary conditions (26 additional images). Multiple calls to ::traverse are required if
 * more images are needed, but \a out may be overwritten each time.
 *
 * If a query sphere overlaps an internal node, the traversal should descend to the left child.
 * If the query sphere does not overlap OR it has reached a leaf node, the traversal should proceed
 * along the rope. Traversal terminates when the LBVHSentinel is reached for the rope.
 */
template<class OutputOpT>
void LBVHTraverser::traverse(OutputOpT& out,
                             const GlobalArray<Scalar4>& spheres,
                             unsigned int N,
                             const LBVH& lbvh,
                             const GlobalArray<Scalar3>& images)
    {
    // kernel uses int32 bitflags for the images, so limit to 32 images
    const unsigned int Nimages = images.getNumElements();
    if (Nimages > 32)
        {
        m_exec_conf->msg->error() << "A maximum of 32 image vectors are supported by LBVH traversers." << std::endl;
        throw std::runtime_error("Too many images (>32) in LBVH traverser.");
        }

    // compress the tree
    compress(lbvh);

    // compressed lbvh data
    ArrayHandle<int4> d_data(m_data, access_location::device, access_mode::read);
    gpu::LBVHCompressedData clbvh;
    clbvh.root = lbvh.getRoot();
    clbvh.data = d_data.data;
    clbvh.lo = m_lbvh_lo.getDeviceFlags();
    clbvh.hi = m_lbvh_hi.getDeviceFlags();
    clbvh.bins = m_bins.getDeviceFlags();

    // traversal data
    ArrayHandle<Scalar4> d_spheres(spheres, access_location::device, access_mode::read);
    ArrayHandle<Scalar3> d_images(images, access_location::device, access_mode::read);

    m_tune_traverse->begin();
    gpu::lbvh_traverse_ropes(out,
                             clbvh,
                             d_spheres.data,
                             d_images.data,
                             Nimages,
                             N,
                             m_tune_traverse->getParam());
    if (m_exec_conf->isCUDAErrorCheckingEnabled()) CHECK_CUDA_ERROR();
    m_tune_traverse->end();
    }
} // end namespace neighbor

#endif // NEIGHBOR_LBVH_TRAVERSER_H_
