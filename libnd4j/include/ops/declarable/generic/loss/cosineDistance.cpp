/*******************************************************************************
 * Copyright (c) 2015-2018 Skymind, Inc.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License, Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 ******************************************************************************/

//
// @author Yurii Shyrma (iuriish@yahoo.com), created on 22.11.2017
//

#include <op_boilerplate.h>
#if NOT_EXCLUDED(OP_cosine_distance_loss)

#include <ops/declarable/CustomOperations.h>
#include <helpers/ShapeUtils.h>

namespace nd4j {
namespace ops  {


//////////////////////////////////////////////////////////////////////////
CUSTOM_OP_IMPL(cosine_distance_loss, 3, 1, false, 0, 2) {
  	
  	auto predictions = INPUT_VARIABLE(0);
    auto weights 	 = INPUT_VARIABLE(1);
    auto labels      = INPUT_VARIABLE(2);    
    
    auto output      = OUTPUT_VARIABLE(0);

    int reductionMode = INT_ARG(0);			// 0 - "none"; 1 - "weighted_sum";  2 - "weighted_mean";  3 - "weighted_sum_by_nonzero_weights"
    int dim = INT_ARG(1);					// axis, dimension should be reduced to unity along this axis
    if(dim < 0)
    	dim += labels->rankOf();

    // labels and predictions must have the same shapes
    REQUIRE_TRUE(labels->isSameShape(predictions), 0, "COSINE_DISTANCE_LOSS OP: labels and predictions arrays must have the same shapes, but got %s and %s correspondingly !", ShapeUtils::shapeAsString(labels).c_str(), ShapeUtils::shapeAsString(predictions).c_str());
    // weights array can be single scalar or has the same rank as labels, and must be broadcastable to labels
    REQUIRE_TRUE(weights->isScalar() || weights->rankOf() == labels->rankOf(), 0, "COSINE_DISTANCE_LOSS OP: weights array should be scalar or have the same rank as labels array, but got %i and %i correspondingly!", weights->rankOf(), labels->rankOf());
	// check whether broadcast operation is possible for weights array    
    REQUIRE_TRUE(weights->isScalar() || ShapeUtils::areShapesBroadcastable(*weights, *labels), 0, "COSINE_DISTANCE_LOSS OP: shapes of weights and labels arrays should be broadcastable, but got weights = %s and labels = %s instead!", ShapeUtils::shapeAsString(weights).c_str(), ShapeUtils::shapeAsString(labels).c_str());
    // input dimension can't be larger than labels/predictions/weights rank
    REQUIRE_TRUE(dim < labels->rankOf(), 0, "COSINE_DISTANCE_LOSS OP: input reduction dimension (got %i) must be < labels rank %i!", dim, labels->rankOf());
    // regard 4 possible reduction modes below
    REQUIRE_TRUE(reductionMode==0 || reductionMode==1 || reductionMode==2 || reductionMode==3, 0, "COSINE_DISTANCE_LOSS OP: reduction mode value is not acceptable, possible values are 0, 1, 2, 3, but got %i instead!", reductionMode);

    NDArray E = 1. - ((*predictions) * (*labels)).reduceAlongDims(reduce::Sum, {dim}, true);

	// perform weights broadcasting/tile to E if it is necessary
	auto weightsBroad = weights;
	if(!weights->isScalar() && !weights->isSameShape(&E))
			weightsBroad = new NDArray(weights->tileToShape(E.getShapeInfo()));	

 	// multiply E on weights
 	E *= (*weightsBroad);
 	 	
	switch (reductionMode) {
		case 0:												// 0 - "none", un-reduced weighted losses with the same shape as labels.
			output->assign(&E);
			break;
		
		case 1: {											// 1 - "weighted_sum", output is scalar and equal to sum of all elements of E array
			output->assign(E.reduceNumber(reduce::Sum));
			break;
		}
		case 2: {											// 2 - "weighted_mean", output is scalar and equal to sum of all elements of E array divided by sum of all elements of weightsBroad array
			NDArray sum;
			if (weights->isScalar())
				sum = *weights * E.lengthOf();
			else 
				sum = weightsBroad->reduceNumber(reduce::Sum);
			
			if (sum.e<double>(0) == 0.)
				*output = 0.;
			else 
				output->assign(E.reduceNumber(reduce::Sum) / sum);
			break;
		}
		case 3: {											// 3 - "weighted_sum_by_nonzero_weights", output is scalar and equal to scalar sum of all elements of E array divided by number of non-zero weights
			Nd4jLong numOfNonZeroWeights = 0;
			if(weights->isScalar()) {
				if(weights->e<double>(0) != 0.)
					numOfNonZeroWeights = E.lengthOf();
			}
			else 
				numOfNonZeroWeights = E.reduceNumber(reduce::CountNonZero).e<Nd4jLong>(0);			

			if (numOfNonZeroWeights == 0)
				*output = 0.;
			else 
				output->assign(E.reduceNumber(reduce::Sum) / numOfNonZeroWeights);		
			
			break;
		}
	}


    STORE_RESULT(*output);

    if(weightsBroad != weights)
    	delete weightsBroad;
	
    return Status::OK();
}

		DECLARE_TYPES(cosine_distance_loss) {
			getOpDescriptor()
					->setAllowedInputTypes(nd4j::DataType::ANY)
					->setAllowedOutputTypes({ALL_FLOATS});
		}

DECLARE_SHAPE_FN(cosine_distance_loss) {

	// labels and predictions must have the same shapes 
	auto predictionsShapeInfo = inputShape->at(0);
    auto labelsShapeInfo  	  = inputShape->at(2);

    // labels and predictions must have the same shapes
    REQUIRE_TRUE(shape::shapeEquals(labelsShapeInfo, predictionsShapeInfo), 0, "COSINE_DISTANCE_LOSS OP: labels and predictions arrays must have the same shapes, but got %s and %s correspondingly !", ShapeUtils::shapeAsString(labelsShapeInfo).c_str(), ShapeUtils::shapeAsString(predictionsShapeInfo).c_str());

    int dim = INT_ARG(1);
    if(dim < 0)
    	dim += labelsShapeInfo[0];
 
 	// evaluate output shapeInfo
    Nd4jLong* outShapeInfo = nullptr;
    if(INT_ARG(0) != 0) {			// in this case output is scalar
        outShapeInfo = ShapeBuilders::createScalarShapeInfo(ArrayOptions::dataType(predictionsShapeInfo), block.getWorkspace());
    }
    else {							// in this case output has the same shape as labels reduced  by dim axis    	
    	std::vector<int> dimensions = {dim};
    	outShapeInfo = ShapeUtils::evalReduceShapeInfo(shape::order(predictionsShapeInfo), dimensions, predictionsShapeInfo, true, false, block.getWorkspace());
    }
    return SHAPELIST(outShapeInfo);

}


// INT_ARG(0) - reduction mode
// INT_ARG(1) - axis, dimension should be reduced to unity along this axis

}
}

#endif