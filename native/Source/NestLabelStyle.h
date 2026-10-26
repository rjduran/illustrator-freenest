#pragma once
#include <stdexcept>
namespace nestai {
enum class LabelColor {Black,Blue,Red,Magenta};
struct LabelRGB {int red,green,blue;};
inline LabelRGB LabelColorRGB(LabelColor color) {
    switch(color) {
        case LabelColor::Black:return {0,0,0};
        case LabelColor::Blue:return {0,0,255};
        case LabelColor::Red:return {255,0,0};
        case LabelColor::Magenta:return {255,0,255};
    }
    throw std::runtime_error("Invalid label color.");
}
}
