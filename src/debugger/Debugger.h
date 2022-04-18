#pragma once

#include "Application.h"
#include "components/CPU.h"
#include "components/MMU.h"
#include <memory>
#include <ostream>

namespace gbcemu {
class Debugger {

  public:
    Debugger(std::shared_ptr<CPU>, std::shared_ptr<MMU>, std::shared_ptr<Application>);
    void run(std::ostream &);

  private:
    std::shared_ptr<Application> m_app;
    std::shared_ptr<CPU> m_cpu;
    std::shared_ptr<MMU> m_mmu;
    bool m_is_in_run_mode;
};
}