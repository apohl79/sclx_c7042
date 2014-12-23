#ifndef SCLX_CYCLE_TASK_H_
#define SCLX_CYCLE_TASK_H_

#include <tasks/timer_task.h>
#include <tasks/worker.h>

#include "sclx_task.h"

class sclx_cycle_task : public tasks::timer_task {
  public:
    sclx_cycle_task(sclx_task* task, double cycle_time) : tasks::timer_task(cycle_time, cycle_time), m_task(task) {}

    bool handle_event(tasks::worker* worker, int) {
        auto now = std::chrono::steady_clock::now();
        auto dif = std::chrono::duration_cast<std::chrono::seconds>(now - m_task->last_update()).count();
        if (dif > 1) {
            tdbg("sclx_cycle_task: no update for " << dif << " seconds" << std::endl);
            m_task->cycle_reset(worker);
        }
        return true;
    }

  private:
    sclx_task* m_task;
};

#endif  // SCLX_CYCLE_TASK_H_
