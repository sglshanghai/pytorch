#pragma once

#include "caffe2/core/logging.h"
#include "caffe2/opt/shape_info.h"
#include "caffe2/proto/caffe2_pb.h"

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace caffe2 {
// This struct stores the max bound size for batch in the general sense.
// max_batch_size is the upper bound of batch_size.
// max_seq_size is the upper bound of length of every item in a batch.
// Upper bound of length of a batch of items should be max_batch_size *
// max_seq_size.
struct CAFFE2_API BoundShapeSpec {
  explicit BoundShapeSpec(int64_t b, int64_t q)
      : max_batch_size(b), max_seq_size(q) {}
  int64_t max_batch_size;
  int64_t max_seq_size;
};

/// \class A class that does bound shape inference given a C2 net. Depending on
/// its type, each op have a maximum shape that it accepts. We define some
/// initial bound for certain dimension, for example max batch size or max
/// sequnce lookup size. And the inference will first infer the input size and
/// then propagates the bound shape down the network. For now the variable part
/// (bound part) is the first dimension of the shape, which usually corresponds
/// to the batch size or sequence lookup size.
class BoundShapeInferencerBase {
 public:
  explicit BoundShapeInferencerBase(const BoundShapeSpec& spec) : spec_(spec) {
    CAFFE_ENFORCE_GE(spec_.max_batch_size, 0);
    CAFFE_ENFORCE_GE(spec_.max_seq_size, 0);
  }

  virtual ~BoundShapeInferencerBase() {}

  // Initializes BoundShapeInferencer and infers bound shape and type.
  // info: shape information of some tensors,
  // e.g. shape information of external input / output tensors;
  // extract_feature_len:
  // indicating whether to extract feature length from SigridTransform
  // and other related operators. When enabled,
  // extracted feature length information will be used to infer tensor shapes.
  virtual void InferBoundShapeAndType(
      const NetDef& net,
      const ShapeInfoMap& info,
      caffe2::Workspace* ws,
      bool extract_feature_len = false) = 0;

  const ShapeInfoMap& shape_info() const {
    return shape_info_;
  }

  /// Print out all the shape info
  std::string PrintShapeInfo() const {
    std::stringstream ss;
    for (const auto& kv : shape_info_) {
      const auto& s = kv.second;
      ss << s.shape.name() << ": dim_type: " << s.getDimType() << ", dims: [";
      for (const auto d : s.shape.dims()) {
        ss << d << ", ";
      }
      ss << "], dtype: " << s.shape.data_type() << "\n";
    }
    return ss.str();
  }

 protected:
  const BoundShapeSpec spec_;
  ShapeInfoMap shape_info_;
  bool extract_feature_len_;
};

class CAFFE2_API BoundShapeInferencer : public BoundShapeInferencerBase {
 public:
  explicit BoundShapeInferencer(const BoundShapeSpec& spec)
      : BoundShapeInferencerBase(spec) {}

  virtual ~BoundShapeInferencer() override {}
  void InferBoundShapeAndType(
      const NetDef& net,
      const ShapeInfoMap& info,
      caffe2::Workspace* ws,
      bool extract_feature_len = false) override;

 protected:
  TensorShape& CheckAndSetTensorBoundShape(
      const std::string& name,
      const std::vector<TensorBoundShape::DimType>& t,
      std::vector<int64_t> bound_dims,
      TensorProto::DataType type,
      bool is_quantized,
      bool allow_existing_shape = false);

  TensorShape& SetTensorBoundShapeIfNotExist(
      const std::string& name,
      const std::vector<TensorBoundShape::DimType>& t,
      std::vector<int64_t> bound_dims,
      TensorProto::DataType type,
      bool is_quantized);

  virtual void InferOps(const OperatorDef& op, caffe2::Workspace* ws);

  void InferConcatInputs(const OperatorDef& op);
  void InferInt8QuantizeInput(const OperatorDef& op);
  void InferElementwiseOpInput(const OperatorDef& op);

  void InferElementwiseOp(const OperatorDef& op);
  void InferGivenTensorFill(const OperatorDef& op);
  void InferSparseLengthsSum(const OperatorDef& op);
  void InferFC(const OperatorDef& op);
  void InferConcat(const OperatorDef& op);
  void InferShape(const OperatorDef& op);
  void InferReshape(const OperatorDef& op);
  void InferLengthsRangeFill(const OperatorDef& op);
  void InferQuantizationTransformation(const OperatorDef& op);

  // Standard shape/type inference using op schema registered shape inference
  // function
  void InferCommonOp(const OperatorDef& op);

  // Initialize private parameters, such as shape_info, extract_feature_len_
  // This is called at the beginning of InferBoundShapeAndType()
  virtual void Initialize(const ShapeInfoMap& info, bool extract_feature_len);

  void EnsureShapeNames(ShapeInfoMap* info) const;

  TensorBoundShape::DimType current_dim_type_{TensorBoundShape_DimType_BATCH};
  int64_t current_max_batch_size_{0};
};

CAFFE2_API std::shared_ptr<BoundShapeInferencerBase> getBoundShapeInferencer(
    const BoundShapeSpec& spec);

C10_DECLARE_SHARED_REGISTRY(
    BoundShapeInferencerRegistry,
    BoundShapeInferencerBase,
    const BoundShapeSpec&);

} // namespace caffe2
