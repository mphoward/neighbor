// Copyright (c) 2018-2019, Michael P. Howard.
// This file is released under the Modified BSD License.

// Maintainer: mphoward

#ifndef NEIGHBOR_LBVH_TRAVERSER_H_
#define NEIGHBOR_LBVH_TRAVERSER_H_

#include "LBVH.h"
#include "TransformOps.h"
#include "TranslateOps.h"

#include "Autotuner.h"
#include "kernels/LBVHTraverserKernels.cuh"

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
 * In this implementation, The LBVH is traversed using a stackless scheme based on skip ropes. The
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
 * The query volumes are flexibly defined by a templated \a QueryOpT. Similarly, the output
 * is flexibly implemented using an \a OutputOpT. Common query ops use box or sphere volumes,
 * while output ops may count neighbors or write a neighbor list.
 *
 * The LBVH is not aware of periodic boundary conditions of a scene, and so by default the
 * LBVHTraverser only intersects the volume directly against the LBVH. However, an additional
 * image list can be specified for ::traverse. The image list specifies *additional* translations
 * to consider, beyond the original volume.
 */
class LBVHTraverser
    {
    public:
        //! Constructor
        /*!
         * \param exec_conf HOOMD-blue execution configuration.
         */
        LBVHTraverser();

        //! Setup LBVH for traversal
        template<class TransformOpT>
        void setup(const TransformOpT& transform, LBVH& lbvh, cudaStream_t stream = 0);

        //! Setup LBVH for traversal
        void setup(LBVH& lbvh, cudaStream_t stream = 0)
            {
            setup(NullTransformOp(), lbvh, stream);
            }

        //! Reset (nullify) the setup
        void reset()
            {
            m_replay = false;
            }

        //! Traverse the LBVH.
        template<class OutputOpT, class QueryOpT, class TransformOpT, class TranslateOpT=SelfOp>
        void traverse(OutputOpT& out,
                      const QueryOpT& query,
                      const TransformOpT& transform,
                      LBVH& lbvh,
                      const TranslateOpT& images = TranslateOpT(),
                      cudaStream_t stream = 0);

        //! Traverse the LBVH.
        template<class OutputOpT, class QueryOpT, class TranslateOpT=SelfOp>
        void traverse(OutputOpT& out,
                      const QueryOpT& query,
                      LBVH& lbvh,
                      const TranslateOpT& images = TranslateOpT(),
                      cudaStream_t stream = 0)
            {
            traverse(out, query, NullTransformOp(), lbvh, images, stream);
            }

        //! Access the compressed LBVH data for traversal
        const thrust::device_vector<int4>& getData() const
            {
            return m_data;
            }

        const gpu::LBVHCompressedData data()
            {
            gpu::LBVHCompressedData clbvh;
            clbvh.root = m_root;
            clbvh.data = thrust::raw_pointer_cast(m_data.data());
            clbvh.lo = thrust::raw_pointer_cast(m_lbvh_lo.data());
            clbvh.hi = thrust::raw_pointer_cast(m_lbvh_hi.data());
            clbvh.bins = thrust::raw_pointer_cast(m_bins.data());

            return clbvh;
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
        int m_root;
        thrust::device_vector<int4> m_data;   //!< Internal representation of the LBVH for traversal
        thrust::device_vector<float3> m_lbvh_lo; //!< Lower bound of tree
        thrust::device_vector<float3> m_lbvh_hi; //!< Upper bound of tree
        thrust::device_vector<float3> m_bins;    //!< Bin size for compression

        std::unique_ptr<Autotuner> m_tune_traverse; //!< Autotuner for traversal kernel
        std::unique_ptr<Autotuner> m_tune_compress; //!< Autotuner for compression kernel

        //! Compresses the lbvh into internal representation
        template<class TransformOpT>
        void compress(LBVH& lbvh, const TransformOpT& transform, cudaStream_t stream);

        bool m_replay;  //!< If true, the compressed structure has already been set explicitly
    };

LBVHTraverser::LBVHTraverser()
    : m_lbvh_lo(1), m_lbvh_hi(1), m_bins(1), m_replay(false)
    {
    m_tune_traverse.reset(new Autotuner(32, 1024, 32, 5, 100000));
    m_tune_compress.reset(new Autotuner(32, 1024, 32, 5, 100000));
    }

/*!
 * \param transform Transformation operation for cached primitive indexes.
 * \param lbvh LBVH to traverse.
 *
 * \tparam TransformOpT The type of transformation operation.
 *
 * This method just calls the compress method on the LBVH, and marks that this has been done
 * internally so that subsequent calls to traverse do not compress. This is useful if the same
 * LBVH is going to be traversed multiple times. It is the caller's responsibility to ensure
 * that the transform op and lbvh do not change between setup and traversal, or the result will
 * be incorrect.
 *
 * To clear a setup, call reset().
 */
template<class TransformOpT>
void LBVHTraverser::setup(const TransformOpT& transform, LBVH& lbvh, cudaStream_t stream)
    {
    if (lbvh.getN() == 0) return;

    compress(lbvh, transform, stream);
    m_replay = true;
    }

/*!
 * \param out Output operation for intersected primitives.
 * \param query Query operation for defining search volumes and overlaps.
 * \param transform Transformation operation for cached primitive indexes.
 * \param lbvh LBVH to traverse.
 * \param images Additional images of \a query volumes to test.
 * \param stream CUDA stream for kernel execution.
 *
 * \tparam OutputOpT The type of output operation.
 * \tparam QueryOpT The type of query operation.
 * \tparam TransformOpT The type of transformation operation.
 *
 * A maximum of 32 \a images are allowed due to the internal representation of the image list
 * in the traversal CUDA kernel. This is more than enough to perform traversal in 3D periodic
 * boundary conditions (26 additional images). Multiple calls to ::traverse are required if
 * more images are needed, but \a out may be overwritten each time depending on the \a OutputOpT.
 *
 * If a query volume overlaps an internal node, the traversal should descend to the left child.
 * If the query volume does not overlap OR it has reached a leaf node, the traversal should proceed
 * along the rope. Traversal terminates when the LBVHSentinel is reached for the rope.
 */
template<class OutputOpT, class QueryOpT, class TransformOpT, class TranslateOpT>
void LBVHTraverser::traverse(OutputOpT& out,
                             const QueryOpT& query,
                             const TransformOpT& transform,
                             LBVH& lbvh,
                             const TranslateOpT& images,
                             cudaStream_t stream)
    {
    // don't traverse with empty lbvh
    if (lbvh.getN() == 0) return;

    // don't traverse with no query objects or images
    if (query.size() == 0 || images.size() == 0) return;

    // kernel uses int32 bitflags for the images, so limit to 32 images
    if (images.size() > 32)
        {
        throw std::runtime_error("A maximum of 32 image vectores are supported by LBVH traversers.");
        }

    // setup if this is not a replay
    if (!m_replay)
        setup(transform, lbvh, stream);

    // compressed lbvh data
    gpu::LBVHCompressedData clbvh = data();

    // traversal data
    m_tune_traverse->begin();
    gpu::lbvh_traverse_ropes(out,
                             clbvh,
                             query,
                             images,
                             m_tune_traverse->getParam(),
                             stream);
    m_tune_traverse->end();
    }

/*!
 * \param lbvh LBVH to compress
 * \param transform Transformation operation for cached primitive indexes.
 * \param stream CUDA stream for kernel execution.
 *
 * \tparam TransformOpT The type of transformation operation.
 *
 * The nodes are compressed according to the scheme described previously. The storage
 * requirements are 16B / node (int4). The components of the int4 are:
 *
 *  - x: bits = 00lo.x[0-9]lo.y[0-9]lo.z[0-9]
 *  - y: bits = 00hi.x[0-9]hi.y[0-9]hi.z[0-9]
 *  - z: left child node (if >= 0) or primitive (if < 0)
 *  - w: rope
 *
 * The bits for the bounding box can be decompressed using:
 *      lo.x = ((unsigned int)node.x >> 20) & 0x3ffu;
 *      lo.y = ((unsigned int)node.x >> 10) & 0x3ffu;
 *      lo.z = ((unsigned int)node.x      ) & 0x3ffu;
 * which simply shifts and masks the low 10 bits. These integer bins should then be scaled by
 * the compressed bin size, which is stored internally.
 *
 * If node.z >= 0, then the current node is an internal node, and traversal should descend
 * to the child (node.z). If node.z < 0, the current node is actually a leaf node. In this case,
 * there is no left child. Instead, ~node.z gives a cached index for the intersected primitive.
 * The value that is stored in the cache is determined by \a transform. Sometimes this could just be
 * the original index of the primitive, but other times it might be useful to apply a mapping to the
 * index to save indirection when the index itself is not of interest.
 */
template<class TransformOpT>
void LBVHTraverser::compress(LBVH& lbvh, const TransformOpT& transform, cudaStream_t stream)
    {
    // resize the internal data array
    const unsigned int num_data = lbvh.getNNodes();
    if (num_data > m_data.size())
        {
        thrust::device_vector<int4> tmp(num_data);
        m_data.swap(tmp);
        }

    // acquire current tree data for reading
    gpu::LBVHData tree = lbvh.data();

    // set root and acquire compressed tree data for writing
    m_root = lbvh.getRoot();
    gpu::LBVHCompressedData ctree = data();

    // compress the data
    m_tune_compress->begin();
    gpu::lbvh_compress_ropes(ctree,
                             transform,
                             tree,
                             lbvh.getNInternal(),
                             lbvh.getNNodes(),
                             m_tune_compress->getParam(),
                             stream);
    m_tune_compress->end();
    }
} // end namespace neighbor

#endif // NEIGHBOR_LBVH_TRAVERSER_H_