#include "core/WizardModel.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace ctt {
namespace {

template <std::size_t Size>
std::size_t NearestIndex(const std::array<int, Size>& values, const int value) noexcept {
    std::size_t bestIndex = 0;
    int bestDistance = std::abs(values.front() - value);
    for (std::size_t index = 1; index < values.size(); ++index) {
        const int distance = std::abs(values[index] - value);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = index;
        }
    }
    return bestIndex;
}

}  // namespace

WizardModel::WizardModel(const std::size_t monitorCount, const int globalContrast)
    : profiles_(std::max<std::size_t>(monitorCount, 1U)),
      globalContrast_(std::clamp(globalContrast, 1000, 2200)),
      globalContrastCandidates_(BuildGlobalContrastCandidates(globalContrast_)) {}

WizardModel::WizardModel(std::vector<ClearTypeProfile> profiles, const int globalContrast)
    : profiles_(std::move(profiles)),
      globalContrast_(std::clamp(globalContrast, 1000, 2200)),
      globalContrastCandidates_(BuildGlobalContrastCandidates(globalContrast_)) {
    if (profiles_.empty()) {
        profiles_.emplace_back();
    }
}

WizardPage WizardModel::CurrentPage() const noexcept { return page_; }
std::size_t WizardModel::CurrentMonitorIndex() const noexcept { return currentMonitor_; }
std::size_t WizardModel::MonitorCount() const noexcept { return profiles_.size(); }

bool WizardModel::IsSamplePage() const noexcept {
    return page_ >= WizardPage::PixelStructure && page_ <= WizardPage::GrayscaleEnhancedContrast;
}

CalibrationStage WizardModel::CurrentStage() const noexcept {
    switch (page_) {
        case WizardPage::PixelStructure:
            return CalibrationStage::PixelStructure;
        case WizardPage::GlobalContrast:
            return CalibrationStage::GlobalContrast;
        case WizardPage::ClearTypeLevel:
            return CalibrationStage::ClearTypeLevel;
        case WizardPage::ContrastCombination:
            return CalibrationStage::ContrastCombination;
        case WizardPage::GrayscaleEnhancedContrast:
        default:
            return CalibrationStage::GrayscaleEnhancedContrast;
    }
}

std::size_t WizardModel::CurrentSampleCount() const noexcept {
    return currentMonitor_ == 0U ? 5U : 4U;
}

std::size_t WizardModel::CurrentSampleNumber() const noexcept {
    switch (page_) {
        case WizardPage::PixelStructure:
            return 1U;
        case WizardPage::GlobalContrast:
            return 2U;
        case WizardPage::ClearTypeLevel:
            return currentMonitor_ == 0U ? 3U : 2U;
        case WizardPage::ContrastCombination:
            return currentMonitor_ == 0U ? 4U : 3U;
        case WizardPage::GrayscaleEnhancedContrast:
            return currentMonitor_ == 0U ? 5U : 4U;
        default:
            return 0U;
    }
}

std::size_t WizardModel::SelectedCandidateIndex() const noexcept {
    if (!IsSamplePage()) {
        return 0;
    }
    if (CurrentStage() == CalibrationStage::GlobalContrast) {
        return NearestIndex(globalContrastCandidates_, globalContrast_);
    }
    return NearestCandidateIndex(CurrentStage(), CurrentProfile());
}

const ClearTypeProfile& WizardModel::CurrentProfile() const noexcept { return profiles_[currentMonitor_]; }
ClearTypeProfile& WizardModel::CurrentProfile() noexcept { return profiles_[currentMonitor_]; }

ClearTypeProfile WizardModel::CurrentRenderingProfile() const noexcept {
    return CurrentProfile();
}

ClearTypeProfile WizardModel::CandidateRenderingProfile(const std::size_t index) const noexcept {
    if (CurrentStage() == CalibrationStage::GlobalContrast) {
        ClearTypeProfile profile = CurrentProfile();
        if (index < globalContrastCandidates_.size()) {
            profile.gammaLevel = globalContrastCandidates_[index];
        }
        return profile;
    }
    return RenderingProfileForCandidate(CurrentProfile(), CurrentStage(), index, globalContrast_);
}

const std::vector<ClearTypeProfile>& WizardModel::Profiles() const noexcept { return profiles_; }
int WizardModel::GlobalContrast() const noexcept { return globalContrast_; }
const ContrastCandidates& WizardModel::GlobalContrastCandidates() const noexcept {
    return globalContrastCandidates_;
}

bool WizardModel::Next() noexcept {
    finishedFromWelcome_ = false;
    switch (page_) {
        case WizardPage::Welcome:
            page_ = WizardPage::Resolution;
            return true;
        case WizardPage::Resolution:
            page_ = WizardPage::PixelStructure;
            return true;
        case WizardPage::PixelStructure:
            page_ = currentMonitor_ == 0U ? WizardPage::GlobalContrast : WizardPage::ClearTypeLevel;
            return true;
        case WizardPage::GlobalContrast:
            for (auto& profile : profiles_) {
                profile.gammaLevel = globalContrast_;
            }
            page_ = WizardPage::ClearTypeLevel;
            return true;
        case WizardPage::ClearTypeLevel:
            page_ = WizardPage::ContrastCombination;
            return true;
        case WizardPage::ContrastCombination:
            page_ = WizardPage::GrayscaleEnhancedContrast;
            return true;
        case WizardPage::GrayscaleEnhancedContrast:
            if (currentMonitor_ + 1U < profiles_.size()) {
                ++currentMonitor_;
                page_ = WizardPage::Resolution;
            } else {
                page_ = WizardPage::Finish;
            }
            return true;
        case WizardPage::Finish:
            return false;
    }
    return false;
}

void WizardModel::FinishNow() noexcept {
    currentMonitor_ = 0;
    page_ = WizardPage::Finish;
    finishedFromWelcome_ = true;
}

bool WizardModel::Back() noexcept {
    switch (page_) {
        case WizardPage::Welcome:
            return false;
        case WizardPage::Resolution:
            if (currentMonitor_ == 0U) {
                page_ = WizardPage::Welcome;
            } else {
                --currentMonitor_;
                page_ = WizardPage::GrayscaleEnhancedContrast;
            }
            return true;
        case WizardPage::PixelStructure:
            page_ = WizardPage::Resolution;
            return true;
        case WizardPage::GlobalContrast:
            page_ = WizardPage::PixelStructure;
            return true;
        case WizardPage::ClearTypeLevel:
            page_ = currentMonitor_ == 0U ? WizardPage::GlobalContrast : WizardPage::PixelStructure;
            return true;
        case WizardPage::ContrastCombination:
            page_ = WizardPage::ClearTypeLevel;
            return true;
        case WizardPage::GrayscaleEnhancedContrast:
            page_ = WizardPage::ContrastCombination;
            return true;
        case WizardPage::Finish:
            if (finishedFromWelcome_) {
                page_ = WizardPage::Welcome;
                finishedFromWelcome_ = false;
            } else {
                currentMonitor_ = profiles_.size() - 1U;
                page_ = WizardPage::GrayscaleEnhancedContrast;
            }
            return true;
    }
    return false;
}

void WizardModel::SelectCandidate(const std::size_t index) noexcept {
    if (!IsSamplePage() || index >= CandidateCount(CurrentStage())) {
        return;
    }
    if (CurrentStage() == CalibrationStage::GlobalContrast) {
        globalContrast_ = globalContrastCandidates_[index];
        for (auto& profile : profiles_) {
            profile.gammaLevel = globalContrast_;
        }
        return;
    }
    ApplyCandidate(CurrentProfile(), CurrentStage(), index);
}

}  // namespace ctt
