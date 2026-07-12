#include "core/WizardModel.h"

#include <algorithm>
#include <utility>

namespace ctt {
namespace {
constexpr WizardPage NextPageWithinMonitor(const WizardPage page) noexcept {
    switch (page) {
        case WizardPage::Resolution:
            return WizardPage::PixelStructure;
        case WizardPage::PixelStructure:
            return WizardPage::Gamma;
        case WizardPage::Gamma:
            return WizardPage::ClearTypeLevel;
        case WizardPage::ClearTypeLevel:
            return WizardPage::TextContrast;
        case WizardPage::TextContrast:
            return WizardPage::EnhancedContrast;
        case WizardPage::EnhancedContrast:
            return WizardPage::MonitorComplete;
        default:
            return page;
    }
}

constexpr WizardPage PreviousPageWithinMonitor(const WizardPage page) noexcept {
    switch (page) {
        case WizardPage::MonitorComplete:
            return WizardPage::EnhancedContrast;
        case WizardPage::EnhancedContrast:
            return WizardPage::TextContrast;
        case WizardPage::TextContrast:
            return WizardPage::ClearTypeLevel;
        case WizardPage::ClearTypeLevel:
            return WizardPage::Gamma;
        case WizardPage::Gamma:
            return WizardPage::PixelStructure;
        case WizardPage::PixelStructure:
            return WizardPage::Resolution;
        default:
            return page;
    }
}
}

WizardModel::WizardModel(const std::size_t monitorCount)
    : profiles_(std::max<std::size_t>(monitorCount, 1U)) {}

WizardModel::WizardModel(std::vector<ClearTypeProfile> profiles)
    : profiles_(std::move(profiles)) {
    if (profiles_.empty()) {
        profiles_.emplace_back();
    }
}

WizardPage WizardModel::CurrentPage() const noexcept { return page_; }
std::size_t WizardModel::CurrentMonitorIndex() const noexcept { return currentMonitor_; }
std::size_t WizardModel::MonitorCount() const noexcept { return profiles_.size(); }
bool WizardModel::IsMonitorCompletePage() const noexcept { return page_ == WizardPage::MonitorComplete; }

bool WizardModel::IsSamplePage() const noexcept {
    return page_ >= WizardPage::PixelStructure && page_ <= WizardPage::EnhancedContrast;
}

CalibrationStage WizardModel::CurrentStage() const noexcept {
    switch (page_) {
        case WizardPage::PixelStructure:
            return CalibrationStage::PixelStructure;
        case WizardPage::Gamma:
            return CalibrationStage::Gamma;
        case WizardPage::ClearTypeLevel:
            return CalibrationStage::ClearTypeLevel;
        case WizardPage::TextContrast:
            return CalibrationStage::TextContrast;
        case WizardPage::EnhancedContrast:
        default:
            return CalibrationStage::EnhancedContrast;
    }
}

std::size_t WizardModel::SelectedCandidateIndex() const noexcept {
    if (!IsSamplePage()) {
        return 0;
    }
    const auto stage = CurrentStage();
    return NearestCandidateIndex(stage, ValueForStage(CurrentProfile(), stage));
}

const ClearTypeProfile& WizardModel::CurrentProfile() const noexcept { return profiles_[currentMonitor_]; }
ClearTypeProfile& WizardModel::CurrentProfile() noexcept { return profiles_[currentMonitor_]; }
const std::vector<ClearTypeProfile>& WizardModel::Profiles() const noexcept { return profiles_; }

bool WizardModel::Next() noexcept {
    finishedFromWelcome_ = false;
    switch (page_) {
        case WizardPage::Welcome:
            page_ = WizardPage::MonitorSelection;
            return true;
        case WizardPage::MonitorSelection:
            page_ = WizardPage::Resolution;
            return true;
        case WizardPage::Resolution:
        case WizardPage::PixelStructure:
        case WizardPage::Gamma:
        case WizardPage::ClearTypeLevel:
        case WizardPage::TextContrast:
        case WizardPage::EnhancedContrast:
            page_ = NextPageWithinMonitor(page_);
            return true;
        case WizardPage::MonitorComplete:
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
        case WizardPage::MonitorSelection:
            page_ = WizardPage::Welcome;
            return true;
        case WizardPage::Resolution:
            if (currentMonitor_ == 0U) {
                page_ = WizardPage::MonitorSelection;
            } else {
                --currentMonitor_;
                page_ = WizardPage::MonitorComplete;
            }
            return true;
        case WizardPage::PixelStructure:
        case WizardPage::Gamma:
        case WizardPage::ClearTypeLevel:
        case WizardPage::TextContrast:
        case WizardPage::EnhancedContrast:
        case WizardPage::MonitorComplete:
            page_ = PreviousPageWithinMonitor(page_);
            return true;
        case WizardPage::Finish:
            if (finishedFromWelcome_) {
                page_ = WizardPage::Welcome;
                finishedFromWelcome_ = false;
            } else {
                page_ = WizardPage::MonitorComplete;
            }
            return true;
    }
    return false;
}

void WizardModel::SelectCandidate(const std::size_t index) noexcept {
    if (IsSamplePage()) {
        ApplyCandidate(CurrentProfile(), CurrentStage(), index);
    }
}

}  // namespace ctt
