/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

#include "main_functions.h"
#include "detection_responder.h"
#include "image_provider.h"
#include "model_settings.h"
#include "model.h"
#include "test_samples.h"
#include "synopsys_wei_gpio.h"
// Globals, used for compatibility with Arduino-style sketches.
hx_drv_gpio_config_t hal_gpio_0;
hx_drv_gpio_config_t hal_gpio_1;
hx_drv_gpio_config_t hal_gpio_2;

namespace {
tflite::ErrorReporter* error_reporter = nullptr;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

// In order to use optimized tensorflow lite kernels, a signed int8_t quantized
// model is preferred over the legacy unsigned model format. This means that
// throughout this project, input images must be converted from unisgned to
// signed format. The easiest and quickest way to convert from unsigned to
// signed 8-bit integers is to subtract 128 from the unsigned value to get a
// signed value.

// An area of memory to use for input, output, and intermediate arrays.
constexpr int kTensorArenaSize = 205 * 1024;
static uint8_t tensor_arena[kTensorArenaSize] = {0};



}  // namespace



// The name of this function is important for Arduino compatibility.
void setup() {
  hal_gpio_0.gpio_pin = HX_DRV_PGPIO_0;
  hal_gpio_0.gpio_direction = HX_DRV_GPIO_INPUT;
  hal_gpio_0.gpio_data = 0 ;
  // Set up logging. Google style is to avoid globals or statics because of
  // lifetime uncertainty, but since this has a trivial destructor it's okay.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  model = tflite::GetModel(exp_facenet_quan_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    TF_LITE_REPORT_ERROR(error_reporter,
                         "Model provided is schema version %d not equal "
                         "to supported version %d.",
                         model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  // Pull in only the operation implementations we need.
  // This relies on a complete list of all the ops needed by this graph.
  // An easier approach is to just use the AllOpsResolver, but this will
  // incur some penalty in code space for op implementations that are not
  // needed by this graph.
  //
  // tflite::AllOpsResolver resolver;
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroMutableOpResolver<8> micro_op_resolver;
  micro_op_resolver.AddConv2D();
  micro_op_resolver.AddMaxPool2D();
  micro_op_resolver.AddFullyConnected();
  micro_op_resolver.AddReshape();
  micro_op_resolver.AddRelu();
  micro_op_resolver.AddSoftmax();
  micro_op_resolver.AddQuantize();
  micro_op_resolver.AddDequantize();

  // Build an interpreter to run the model with.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroInterpreter static_interpreter(
      model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
    return;
  }

  // Get information about the memory area to use for the model's input.
  input = interpreter->input(0);
  output = interpreter->output(0);

  // Obtain quantization parameters for result dequantization
}

// The name of this function is important for Arduino compatibility.
void loop()
{
  int32_t test_cnt = 0;
  int32_t correct_cnt = 0;

  float scale = input->params.scale;
  int32_t zero_point = input->params.zero_point;
  uint8_t key_data;
  //Get image from camera and put to mdoel input 
  //hx_drv_uart_print("Wait for user press key: [A] \n");
  //hx_drv_uart_print("hello\n");
    //hx_drv_uart_getchar()
  //hx_drv_uart_print("hi :%d, %d\n",kNumCols,kNumRows);
  if (kTfLiteOk != GetImage(error_reporter, kNumCols, kNumRows, kNumChannels,
                            input->data.int8)) {
    TF_LITE_REPORT_ERROR(error_reporter, "Image capture failed.");
  }
  uint8_t * img_ptr;
  //hx_drv_uart_print("hi\n");
  img_ptr = (uint8_t *)input->data.uint8;
  for(int i = 0; i<48 * 48;i = i + 1){
    
    *img_ptr = *img_ptr;
    
    //hx_drv_uart_print("%3d,",*img_ptr);
    img_ptr = img_ptr + 1;

  }
  //hx_drv_uart_print("\n");
  //Run the model with the input
  if (kTfLiteOk != interpreter->Invoke()) {
    TF_LITE_REPORT_ERROR(error_reporter, "Invoke failed.");
  }

  TfLiteTensor* output = interpreter->output(0);
  RespondToDetection(error_reporter,output->data.int8);

  // Process the inference results.
  /*
  for (int8_t i = 0; i < kCategoryCount; i++){
    int8_t score = output->data.uint8[i];
    RespondToDetection(error_reporter,score, i);
    
    //hx_drv_uart_print("============================\n");   
  }
  /*
  //TF_LITE_REPORT_ERROR(error_reporter, "======================");
  // Invoke interpreter for each test sample and process results
/* Original eating test data
  for (int j = 0; j < kNumSamples; j++) 
  {
    
  
    TF_LITE_REPORT_ERROR(error_reporter, "Test sample[%d] Start Reading and round values to either -128 or 127\n", j);
    // Perform image thinning (round values to either -128 or 127)
    // Write image to input data
    for (int i = 0; i < kImageSize; i++) {
      input->data.int8[i] = (test_samples[j].image[i] <= 210) ? -128 : 127;
    }
  
    TF_LITE_REPORT_ERROR(error_reporter, "Test sample[%d] Start Invoking\n", j);
    // Run the model on this input and make sure it succeeds.
    if (kTfLiteOk != interpreter->Invoke()) {
      TF_LITE_REPORT_ERROR(error_reporter, "Invoke failed.");
    }
  
    TF_LITE_REPORT_ERROR(error_reporter, "Test sample[%d] Start Finding Max Value\n", j);
    // Get max result from output array and calculate confidence
    int8_t* results_ptr = output->data.int8;
    int result = std::distance(results_ptr, std::max_element(results_ptr, results_ptr + 26));
    float confidence = ((results_ptr[result] - zero_point)*scale + 1) / 2;
    const char *status = result == test_samples[j].label ? "SUCCESS" : "FAIL";

    if(result == test_samples[j].label)
      correct_cnt ++;
    test_cnt ++;

    TF_LITE_REPORT_ERROR(error_reporter, 
      "Test sample \"%s\":\n"
      "Predicted %s (%d%%) - %s\n",
      kCategoryLabels[test_samples[j].label], 
      kCategoryLabels[result], (int)(confidence * 100), status);


    TF_LITE_REPORT_ERROR(error_reporter, "Test sample[%d] Start Reading\n", j);
    // Write image to input data
    for (int i = 0; i < kImageSize; i++) {
      input->data.int8[i] = test_samples[j].image[i] - 128;
    }

    TF_LITE_REPORT_ERROR(error_reporter, "Test sample[%d] Start Invoking\n", j);
    // Run the model on this input and make sure it succeeds.
    if (kTfLiteOk != interpreter->Invoke()) {
      TF_LITE_REPORT_ERROR(error_reporter, "Invoke failed.");
    }
  
    TF_LITE_REPORT_ERROR(error_reporter, "Test sample[%d] Start Finding Max Value\n", j);
    // Get max result from output array and calculate confidence
    results_ptr = output->data.int8;
    result = std::distance(results_ptr, std::max_element(results_ptr, results_ptr + 26));
    confidence = ((results_ptr[result] - zero_point)*scale + 1) / 2;
    status = result == test_samples[j].label ? "SUCCESS" : "FAIL";

    if(result == test_samples[j].label)
      correct_cnt ++;
    test_cnt ++;

    TF_LITE_REPORT_ERROR(error_reporter, 
      "Test sample \"%s\":\n"
      "Predicted %s (%d%%) - %s\n",
      kCategoryLabels[test_samples[j].label], 
      kCategoryLabels[result], (int)(confidence * 100), status);      
      
    TF_LITE_REPORT_ERROR(error_reporter, "Test sample[%d] Finsih\n\n", j);
  }
*/
  //TF_LITE_REPORT_ERROR(error_reporter, "Correct Rate = %d / %d\n\n", correct_cnt, test_cnt);
  //while(1);

  
}