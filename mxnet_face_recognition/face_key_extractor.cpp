#include "face_key_extractor.hpp"

#include "../libs/mxnet/common.hpp"
#include "face_key.hpp"
#include "face_key_extractor_params.hpp"

#include <opencv2/imgproc.hpp>

#include <mxnet-cpp/MxNetCpp.h>

using namespace mxnet::cpp;

namespace mxnet_tool{

namespace{

size_t constexpr step_per_feature = 512;

}

face_key_extractor::face_key_extractor(face_key_extractor_params const &params) :    
    params_(std::make_unique<face_key_extractor_params>(params))
{
    executor_ = mxnet_aux::create_executor(params.model_params_, params.model_symbols_,
                                           params.context_, params.shape_);
    image_vector_.resize(params_->shape_.Size());
}

void face_key_extractor::dlib_matrix_to_float_array(dlib_const_images_ptr const &rgb_image)
{
    size_t index = 0;
    for(size_t i = 0; i != rgb_image.size(); ++i){
        for(size_t ch = 0; ch != 3; ++ch){
            for(long row = 0; row != rgb_image[i]->nr(); ++row){
                for(long col = 0; col != rgb_image[i]->nc(); ++col){
                    auto const &pix = (*rgb_image[i])(row, col);
                    switch(ch){
                    case 0:
                        image_vector_[index++] = pix.red;
                        break;
                    case 1:
                        image_vector_[index++] = pix.green;
                        break;
                    case 2:
                        image_vector_[index++] = pix.blue;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }
}

face_key face_key_extractor::forward(const dlib::matrix<dlib::rgb_pixel> &input)
{
    std::vector<dlib::matrix<dlib::rgb_pixel> const*> faces;
    faces.emplace_back(&input);
    dlib_matrix_to_float_array(faces);
    return forward(image_vector_, 1)[0];
}

std::vector<face_key> face_key_extractor::forward(const std::vector<dlib::matrix<dlib::rgb_pixel> > &input)
{
    if(input.empty()){
        return {};
    }

    auto const forward_count = static_cast<size_t>(std::ceil(input.size() / static_cast<float>(params_->shape_[0])));
    std::vector<face_key> result;    
    for(size_t i = 0, index = 0; i != forward_count; ++i){
        dlib_const_images_ptr faces;
        for(size_t j = 0; j != params_->shape_[0] && index < input.size(); ++j){
            faces.emplace_back(&input[index++]);
        }
        dlib_matrix_to_float_array(faces);
        auto features = forward(image_vector_, static_cast<size_t>(faces.size()));
        std::move(std::begin(features), std::end(features), std::back_inserter(result));
    }

    return result;
}

std::vector<face_key> face_key_extractor::forward(const std::vector<float> &input, size_t batch_size)
{
    executor_->arg_dict()["data"].SyncCopyFromCPU(input.data(), input.size());
    executor_->arg_dict()["data1"] = batch_size;
    executor_->Forward(false);
    std::vector<face_key> result;
    if(!executor_->outputs.empty()){
        auto features = executor_->outputs[0].Copy(Context(kCPU, 0));
        Shape const shape(1, step_per_feature);
        features.WaitToRead();
        for(size_t i = 0; i != batch_size; ++i){
            NDArray feature(features.GetData() + i * step_per_feature, shape, Context(kCPU, 0));
            //python scripts perform this step, but I don't find any difference even remove it
            //cv::Mat temp(1, 512, CV_32F, const_cast<mx_float*>(features.GetData()), 0);
            //cv::normalize(temp, temp, 1, 0, cv::NORM_L2);
            result.emplace_back(std::move(feature));
        }
        return result;
    }

    return result;
}

}
