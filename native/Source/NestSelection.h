#pragma once
#include <vector>
#include <stdexcept>
namespace nestai {
enum class SelectedKind { Path, CompoundPath, Symbol, LayerContainer, Unsupported };
// Illustrator includes selected layer containers in addition to the actual paths.
// Ignore only those synthetic containers; an ordinary group remains unsupported.
inline std::vector<size_t> SelectedPathIndices(const std::vector<SelectedKind>& selected) {
    std::vector<size_t> paths;
    for(size_t i=0;i<selected.size();++i) {
        if(selected[i]==SelectedKind::LayerContainer) continue;
        if(selected[i]!=SelectedKind::Path&&selected[i]!=SelectedKind::CompoundPath&&selected[i]!=SelectedKind::Symbol)
            throw std::runtime_error("Select closed paths, compound paths with holes, or symbol instances. Ungroup ordinary groups first.");
        paths.push_back(i);
    }
    if(paths.empty()||paths.size()>500) throw std::runtime_error("Select between 1 and 500 parts.");
    return paths;
}
}
