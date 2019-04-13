#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_SELECT_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_SELECT_STEP_H_

#include "eval/eval/evaluator_core.h"
#include "eval/public/activation.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Factory method for Select - based Execution step
util::StatusOr<std::unique_ptr<ExpressionStep>> CreateSelectStep(
    const google::api::expr::v1alpha1::Expr::Select* select_expr,
    const google::api::expr::v1alpha1::Expr* expr, absl::string_view select_path);

// Factory method for Select - based Execution step
util::StatusOr<std::unique_ptr<ExpressionStep>> CreateSelectStep(
    const google::api::expr::v1alpha1::Expr::Select* select_expr,
    const google::api::expr::v1alpha1::Expr* expr);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_SELECT_STEP_H_
