#pragma once

#include "components/CPU.h"
#include "components/MMU.h"
#include <ostream>

namespace gbcemu {
class Debugger {

  public:
    Debugger(std::shared_ptr<CPU>, std::shared_ptr<MMU>);
    void run(std::ostream &);

  private:
    std::shared_ptr<CPU> m_cpu;
    std::shared_ptr<MMU> m_mmu;
    bool m_step_mode;
};
}