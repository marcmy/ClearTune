#pragma once

#include "core/Candidates.h"
#include "core/Profile.h"

#include <cstddef>
#include <vector>

namespace ctt {

enum class WizardPage {
    Welcome,
    Resolution,
    PixelStructure,
    EnhancedContrast,
    ClearTypeLevel,
    ContrastCombination,
    GrayscaleEnhancedContrast,
    Finish,
};

class WizardModel {
public:
    explicit WizardModel(std::size_t monitorCount);
    explicit WizardModel(std::vector<ClearTypeProfile> profiles);

    [[nodiscard]] WizardPage CurrentPage() const noexcept;
    [[nodiscard]] std::size_t CurrentMonitorIndex() const noexcept;
    [[nodiscard]] std::size_t MonitorCount() const noexcept;
    [[nodiscard]] bool IsSamplePage() const noexcept;
    [[nodiscard]] CalibrationStage CurrentStage() const noexcept;
    [[nodiscard]] std::size_t SelectedCandidateIndex() const noexcept;
    [[nodiscard]] const ClearTypeProfile& CurrentProfile() const noexcept;
    [[nodiscard]] ClearTypeProfile& CurrentProfile() noexcept;
    [[nodiscard]] const std::vector<ClearTypeProfile>& Profiles() const noexcept;

    bool Next() noexcept;
    void FinishNow() noexcept;
    bool Back() noexcept;
    void SelectCandidate(std::size_t index) noexcept;

private:
    std::vector<ClearTypeProfile> profiles_;
    std::size_t currentMonitor_{0};
    WizardPage page_{WizardPage::Welcome};
    bool finishedFromWelcome_{false};
};

}  // namespace ctt
