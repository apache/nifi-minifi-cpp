/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "TFConvertImageToTensor.h"
#include <core/ProcessContext.h>
#include <core/ProcessSession.h>
#include <tensorflow/cc/ops/standard_ops.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property TFConvertImageToTensor::ImageFormat(
    core::PropertyBuilder::createProperty("Input Format")
        ->withDescription(
            "The format of the input image (PNG or RAW). RAW is RGB24.")
        ->withDefaultValue("")
        ->build());

core::Property TFConvertImageToTensor::InputWidth(
    core::PropertyBuilder::createProperty("Input Width")
        ->withDescription("The width, in pixels, of the input image.")
        ->withDefaultValue("")
        ->build());

core::Property TFConvertImageToTensor::InputHeight(
    core::PropertyBuilder::createProperty("Input Height")
        ->withDescription("The height, in pixels, of the input image.")
        ->withDefaultValue("")
        ->build());

core::Property TFConvertImageToTensor::OutputWidth(
    core::PropertyBuilder::createProperty("Output Width")
        ->withDescription("The width, in pixels, of the output image.")
        ->withDefaultValue("")
        ->build());

core::Property TFConvertImageToTensor::OutputHeight(
    core::PropertyBuilder::createProperty("Output Height")
        ->withDescription("The height, in pixels, of the output image.")
        ->withDefaultValue("")
        ->build());

core::Property TFConvertImageToTensor::NumChannels(
    core::PropertyBuilder::createProperty("Channels")
        ->withDescription("The number of channels (e.g. 3 for RGB, 4 for RGBA) "
                          "in the input image")
        ->withDefaultValue("3")
        ->build());

core::Property TFConvertImageToTensor::CropOffsetX(
    core::PropertyBuilder::createProperty("Crop Offset X")
        ->withDescription("The X (horizontal) offset, in pixels, to crop the "
                          "input image (relative to top-left corner).")
        ->withDefaultValue("")
        ->build());

core::Property TFConvertImageToTensor::CropOffsetY(
    core::PropertyBuilder::createProperty("Crop Offset Y")
        ->withDescription("The Y (vertical) offset, in pixels, to crop the "
                          "input image (relative to top-left corner).")
        ->withDefaultValue("")
        ->build());

core::Property TFConvertImageToTensor::CropSizeX(
    core::PropertyBuilder::createProperty("Crop Size X")
        ->withDescription(
            "The X (horizontal) size, in pixels, to crop the input image.")
        ->withDefaultValue("")
        ->build());

core::Property TFConvertImageToTensor::CropSizeY(
    core::PropertyBuilder::createProperty("Crop Size Y")
        ->withDescription(
            "The Y (vertical) size, in pixels, to crop the input image.")
        ->withDefaultValue("")
        ->build());

core::Relationship TFConvertImageToTensor::Success(  // NOLINT
    "success",
    "Successful graph application outputs");
core::Relationship TFConvertImageToTensor::Failure(  // NOLINT
    "failure",
    "Failures which will not work if retried");

void TFConvertImageToTensor::initialize() {
  std::set<core::Property> properties;
  properties.insert(ImageFormat);
  properties.insert(InputWidth);
  properties.insert(InputHeight);
  properties.insert(OutputWidth);
  properties.insert(OutputHeight);
  properties.insert(NumChannels);
  setSupportedProperties(std::move(properties));

  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Failure);
  setSupportedRelationships(std::move(relationships));
}

void TFConvertImageToTensor::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /*sessionFactory*/) {
  context->getProperty(ImageFormat.getName(), input_format_);

  if (input_format_.empty()) {
    logger_->log_error("Invalid image format");
  }

  std::string val;

  if (context->getProperty(InputWidth.getName(), val)) {
    core::Property::StringToInt(val, input_width_);
  } else {
    logger_->log_error("Invalid Input Width");
  }

  if (context->getProperty(InputHeight.getName(), val)) {
    core::Property::StringToInt(val, input_height_);
  } else {
    logger_->log_error("Invalid Input Height");
  }

  if (context->getProperty(OutputWidth.getName(), val)) {
    core::Property::StringToInt(val, output_width_);
  } else {
    logger_->log_error("Invalid Output Width");
  }

  if (context->getProperty(OutputHeight.getName(), val)) {
    core::Property::StringToInt(val, output_height_);
  } else {
    logger_->log_error("Invalid output height");
  }

  if (context->getProperty(NumChannels.getName(), val)) {
    core::Property::StringToInt(val, num_channels_);
  } else {
    logger_->log_error("Invalid channel count");
  }

  do_crop_ = true;

  if (context->getProperty(CropOffsetX.getName(), val)) {
    core::Property::StringToInt(val, crop_offset_x_);
  } else {
    do_crop_ = false;
  }

  if (context->getProperty(CropOffsetY.getName(), val)) {
    core::Property::StringToInt(val, crop_offset_y_);
  } else {
    do_crop_ = false;
  }

  if (context->getProperty(CropSizeX.getName(), val)) {
    core::Property::StringToInt(val, crop_size_x_);
  } else {
    do_crop_ = false;
  }

  if (context->getProperty(CropSizeY.getName(), val)) {
    core::Property::StringToInt(val, crop_size_y_);
  } else {
    do_crop_ = false;
  }

  if (do_crop_) {
    logger_->log_info("Input images will be cropped at %d, %d by %d, %d pixels",
                      crop_offset_x_,
                      crop_offset_y_,
                      crop_size_x_,
                      crop_size_y_);
  }
}

void TFConvertImageToTensor::onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/,
                                       const std::shared_ptr<core::ProcessSession>& session) {
  auto flow_file = session->get();

  if (!flow_file) {
    return;
  }

  try {
    // Use an existing context, if one is available
    std::shared_ptr<TFContext> ctx;

    if (tf_context_q_.try_dequeue(ctx)) {
      logger_->log_debug("Using available TensorFlow context");
    }

    std::string input_tensor_name = "input";
    std::string output_tensor_name = "output";

    if (!ctx) {
      logger_->log_info("Creating new TensorFlow context");
      tensorflow::SessionOptions options;
      ctx = std::make_shared<TFContext>();
      ctx->tf_session.reset(tensorflow::NewSession(options));

      auto root = tensorflow::Scope::NewRootScope();
      auto input = tensorflow::ops::Placeholder(root.WithOpName(input_tensor_name), tensorflow::DT_UINT8);

      // Cast pixel values to floats
      auto float_caster = tensorflow::ops::Cast(root.WithOpName("float_caster"), input, tensorflow::DT_FLOAT);

      int crop_offset_x = 0;
      int crop_offset_y = 0;
      int crop_size_x = input_width_;
      int crop_size_y = input_height_;

      if (do_crop_) {
        crop_offset_x = crop_offset_x_;
        crop_offset_y = crop_offset_y_;
        crop_size_x = crop_size_x_;
        crop_size_y = crop_size_y_;
      }

      tensorflow::ops::Slice cropped = tensorflow::ops::Slice(root.WithOpName("crop"),
                                                              float_caster,
                                                              {crop_offset_y,
                                                               crop_offset_x,
                                                               0},
                                                              {crop_size_y,
                                                               crop_size_x,
                                                               num_channels_});

      // Expand into batches (of size 1)
      auto dims_expander = tensorflow::ops::ExpandDims(root, cropped, 0);

      // Resize tensor to output dimensions
      auto resize = tensorflow::ops::ResizeBilinear(
          root, dims_expander,
          tensorflow::ops::Const(root.WithOpName("resize"), {output_height_, output_width_}));

      // Normalize tensor from 0-255 pixel values to 0.0-1.0 values
      auto output = tensorflow::ops::Div(root.WithOpName(output_tensor_name),
                                         tensorflow::ops::Sub(root, resize, {0.0f}),
                                         {255.0f});
      tensorflow::GraphDef graph_def;
      {
        auto status = root.ToGraphDef(&graph_def);

        if (!status.ok()) {
          std::string msg = "Failed to create TensorFlow graph: ";
          msg.append(status.ToString());
          throw std::runtime_error(msg);
        }
      }

      {
        auto status = ctx->tf_session->Create(graph_def);

        if (!status.ok()) {
          std::string msg = "Failed to create TensorFlow session: ";
          msg.append(status.ToString());
          throw std::runtime_error(msg);
        }
      }
    }

    // Apply graph
    // Read input tensor from flow file
    tensorflow::Tensor img_tensor(tensorflow::DT_UINT8, {input_height_, input_width_, num_channels_});
    ImageReadCallback tensor_cb(&img_tensor);
    session->read(flow_file, &tensor_cb);
    std::vector<tensorflow::Tensor> outputs;
    auto status = ctx->tf_session->Run({{input_tensor_name, img_tensor}}, {output_tensor_name + ":0"}, {}, &outputs);

    if (!status.ok()) {
      std::string msg = "Failed to apply TensorFlow graph: ";
      msg.append(status.ToString());
      throw std::runtime_error(msg);
    }

    // Create output flow file for each output tensor
    for (const auto &output : outputs) {
      auto tensor_proto = std::make_shared<tensorflow::TensorProto>();
      output.AsProtoTensorContent(tensor_proto.get());
      logger_->log_info("Writing output tensor flow file");
      TensorWriteCallback write_cb(tensor_proto);
      session->write(flow_file, &write_cb);
      session->transfer(flow_file, Success);
    }

    // Make context available for use again
    if (tf_context_q_.size_approx() < getMaxConcurrentTasks()) {
      logger_->log_debug("Releasing TensorFlow context");
      tf_context_q_.enqueue(ctx);
    } else {
      logger_->log_info("Destroying TensorFlow context because it is no longer needed");
    }
  } catch (std::exception &exception) {
    logger_->log_error("Caught Exception %s", exception.what());
    session->transfer(flow_file, Failure);
    this->yield();
  } catch (...) {
    logger_->log_error("Caught Exception");
    session->transfer(flow_file, Failure);
    this->yield();
  }
}

int64_t TFConvertImageToTensor::ImageReadCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  if (tensor_->AllocatedBytes() < stream->size()) {
    throw std::runtime_error("Tensor is not big enough to hold FlowFile bytes");
  }
  const auto num_read = stream->read(tensor_->flat<unsigned char>().data(), stream->size());
  if (num_read != stream->size()) {
    throw std::runtime_error("TensorReadCallback failed to fully read flow file input stream");
  }
  return num_read;
}

int64_t TFConvertImageToTensor::TensorWriteCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  auto tensor_proto_buf = tensor_proto_->SerializeAsString();
  auto num_wrote = stream->write(reinterpret_cast<uint8_t *>(&tensor_proto_buf[0]),
                                     static_cast<int>(tensor_proto_buf.size()));

  if (num_wrote != tensor_proto_buf.size()) {
    std::string msg = "TensorWriteCallback failed to fully write flow file output stream; Expected ";
    msg.append(std::to_string(tensor_proto_buf.size()));
    msg.append(" and wrote ");
    msg.append(std::to_string(num_wrote));
    throw std::runtime_error(msg);
  }

  return num_wrote;
}

REGISTER_RESOURCE(TFConvertImageToTensor, "Converts the input image file into a tensor protobuf. The image will be resized to the given output tensor dimensions."); // NOLINT

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
