#pragma once
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>
#include <stdexcept>
namespace nestai {
struct OutputRect {double left,bottom,right,top;};
inline OutputRect OutputArtboard(OutputRect active,const std::vector<OutputRect>& existing,
                                double sourceRight,double width,double height,double gap,OutputRect canvas) {
    if(!std::isfinite(width)||!std::isfinite(height)||!std::isfinite(gap)||width<=0||height<=0||gap<0||
       width>canvas.right-canvas.left||height>canvas.top-canvas.bottom)
        throw std::runtime_error("The output sheet and its label are larger than the available canvas. Use a smaller sheet or label.");
    auto obstacles=existing;
    std::sort(obstacles.begin(),obstacles.end(),[](OutputRect a,OutputRect b){return a.left<b.left;});
    OutputRect result{};
    auto row=[&](double top,double left) {
        if(top>canvas.top||top-height<canvas.bottom) return false;
        left=std::max(left,canvas.left);
        for(auto board:obstacles) {
            if(top-height<board.top+gap&&top+gap>board.bottom&&
               left<board.right+gap&&left+width+gap>board.left)
                left=board.right+gap;
        }
        if(left+width>canvas.right) return false;
        result={left,top-height,left+width,top};return true;
    };
    // First keep the familiar top-aligned position to the right of the source.
    if(row(active.top,std::max(active.right,sourceRight)+gap)) return result;
    // Start every wrapped row under the initial artboard, never at the canvas edge.
    const double firstRowTop=active.bottom-gap;
    std::vector<double> tops{firstRowTop};
    for(auto board:obstacles) if(board.bottom-gap<=firstRowTop) tops.push_back(board.bottom-gap);
    std::sort(tops.begin(),tops.end(),std::greater<double>());
    tops.erase(std::unique(tops.begin(),tops.end()),tops.end());
    for(double top:tops) if(row(top,active.left)) return result;
    throw std::runtime_error("There is not enough canvas space to the right of or below the initial artboard for all output sheets. Move the source artboard up or left, use fewer sheets, or use smaller sheets.");
}
}
