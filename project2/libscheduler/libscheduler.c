/** @file libscheduler.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libscheduler.h"
#include "../libpriqueue/libpriqueue.h"

/**
 * Global priqueue
 */
priqueue_t q;

int FCFScompare(const void * a, const void * b)
{
  return 1;
}

int SJFcompare(const void * a, const void * b)
{
  return ((*(job_t*)a).processTime - (*(job_t*)b).processTime);
}

int PRIcompare(const void * a, const void * b)
{
  int compare = (*(job_t *)a).priority - (*(job_t *)b).priority;

  if(compare == 0)
    return (*(job_t *)a).arrivalTime - (*(job_t *)b).arrivalTime;
  else
    return compare;
}


/**
  Initalizes the scheduler.
 
  Assumptions:
    - You may assume this will be the first scheduler function called.
    - You may assume this function will be called once once.
    - You may assume that cores is a positive, non-zero number.
    - You may assume that scheme is a valid scheduling scheme.

  @param cores the number of cores that is available by the scheduler. These cores will be known as core(id=0), core(id=1), ..., core(id=cores-1).
  @param scheme  the scheduling scheme that should be used. This value will be one of the six enum values of scheme_t
*/
void scheduler_start_up(int cores, scheme_t scheme)
{
  m_cores = cores;
  m_coreArr = malloc(cores * sizeof(job_t));

  m_waitingTime = 0.0;
  m_responseTime = 0.0;
  m_turnaroundTime = 0.0;
  m_numJobs = 0;

  // Initializes core array so that all cores are in unused state at startup
  int i;
  for (i = 0; i < cores; i++)
  {
    m_coreArr[i] = NULL;
  }

  m_type = scheme;

  if (m_type == FCFS || m_type == RR)
  {
    priqueue_init(&q, FCFScompare);
  }
  else if (m_type == SJF || m_type == PSJF)
  {
    priqueue_init(&q, SJFcompare);
  }
  else if (m_type == PRI || m_type == PPRI)
  {
    priqueue_init(&q, PRIcompare);
  }
}


/**
  Determines if there are any idle cores, and if so, returns the index of the first idle core

  @return index of the first idle core
  @return -1 if no cores are available
 */
int scheduler_idle_core_finder()
{
  int i;
  for (i = 0; i < m_cores; i++)
  {
    if (m_coreArr[i] == NULL)
    {
      return i;
    }
  }

  return -1;
}


/**
  Called when a new job arrives.
 
  If multiple cores are idle, the job should be assigned to the core with the
  lowest id.
  If the job arriving should be scheduled to run during the next
  time cycle, return the zero-based index of the core the job should be
  scheduled on. If another job is already running on the core specified,
  this will preempt the currently running job.
  Assumptions:
    - You may assume that every job wil have a unique arrival time.

  @param job_number a globally unique identification number of the job arriving.
  @param time the current time of the simulator.
  @param running_time the total number of time units this job will run before it will be finished.
  @param priority the priority of the job. (The lower the value, the higher the priority.)
  @return index of core job should be scheduled on
  @return -1 if no scheduling changes should be made. 
 */
int scheduler_new_job(int job_number, int time, int running_time, int priority)
{
  int firstIdleCoreFound = scheduler_idle_core_finder();

  job_t *temp = malloc(sizeof(job_t));

  temp->pid = job_number;
  temp->arrivalTime = time;
  temp->priority = priority;
  temp->originalProcessTime = running_time;
  temp->processTime = running_time;
  temp->responseTime = -1;

  if (firstIdleCoreFound != -1)
  {
    // Signal that the core at firstIdleCoreFound is being used
    m_coreArr[firstIdleCoreFound] = temp;
    m_coreArr[firstIdleCoreFound]->responseTime = time - m_coreArr[firstIdleCoreFound]->arrivalTime;

    if (m_type == PSJF)
    {
      temp->lastCheckedTime = time;
    }
    return firstIdleCoreFound;
  }
  else if (m_type == PSJF)
  {
    // Preemptive portion of SFJ
    // Search through all of the cores, and retrieve the longest run time of a given job
    // if the longest run time is longer than the run time of the job trying to be added, replace the job and put it back on the queue
    int i;
    int longestRunTimeFound = -1;
    int indexOfJobWithLongestRuntime;

    for (i = 0; i < m_cores; i++)
    {
      // Update this job's processTime
      m_coreArr[i]->processTime = m_coreArr[i]->processTime - (time - m_coreArr[i]->lastCheckedTime);
      m_coreArr[i]->lastCheckedTime = time;

      if (m_coreArr[i]->processTime > longestRunTimeFound)
      {
        longestRunTimeFound = m_coreArr[i]->processTime;
        indexOfJobWithLongestRuntime = i;
      }
    }

    // If the job found with the longest remaining runtime is longer than job trying to be added, replace the job on the core
    if (longestRunTimeFound > running_time)
    {
      // If we just scheduled this job and it's getting pre-empted, reset the response time
      if(m_coreArr[indexOfJobWithLongestRuntime]->responseTime == time - m_coreArr[indexOfJobWithLongestRuntime]->arrivalTime)
      {
        m_coreArr[indexOfJobWithLongestRuntime]->responseTime = -1;
      }

      priqueue_offer(&q, m_coreArr[indexOfJobWithLongestRuntime]);
      m_coreArr[indexOfJobWithLongestRuntime] = temp;

      if(m_coreArr[indexOfJobWithLongestRuntime]->responseTime == -1)
      {
        m_coreArr[indexOfJobWithLongestRuntime]->responseTime = time - m_coreArr[indexOfJobWithLongestRuntime]->arrivalTime;
      }
      return indexOfJobWithLongestRuntime;
    }
  }
  else if (m_type == PPRI)
  {
    // No idle cores, preempt a job with lower priority, if any
    int i, lowestPriSoFar = m_coreArr[0]->priority, lowestPriCore = 0;

    for(i = 0; i < m_cores; i++) {
      // Check first for lower priority
      if(m_coreArr[i]->priority > lowestPriSoFar)
      {
        lowestPriSoFar = m_coreArr[i]->priority;
        lowestPriCore = i;
      }
      // They have the same priority, check for a larger arrival time
      else if(m_coreArr[i]->priority == lowestPriSoFar 
          && m_coreArr[i]->arrivalTime > m_coreArr[lowestPriCore]->arrivalTime)
      {
        lowestPriCore = i;
      }
    }

    // We have the lowest priority and the core it's running on, compare to new job
    if(lowestPriSoFar > temp->priority)
    {
      // If we just scheduled this job and it's getting pre-empted, reset the response time
      if(m_coreArr[lowestPriCore]->responseTime == time - m_coreArr[lowestPriCore]->arrivalTime)
      {
        m_coreArr[lowestPriCore]->responseTime = -1;
      }

      // Send the job running on the found core to the priqueue, put temp in its place
      priqueue_offer(&q, m_coreArr[lowestPriCore]);
      m_coreArr[lowestPriCore] = temp;

      if(m_coreArr[lowestPriCore]->responseTime == -1)
      {
        m_coreArr[lowestPriCore]->responseTime = time - m_coreArr[lowestPriCore]->arrivalTime;
      }

      return lowestPriCore;
    }
    // Else, put temp on the queue and signal no scheduling changes
  }

  // If at this step, no scheduling changes should be made
  priqueue_offer(&q, temp);
  return -1;
}


/**
  Called when a job has completed execution.
 
  The core_id, job_number and time parameters are provided for convenience. You may be able to calculate the values with your own data structure.
  If any job should be scheduled to run on the core free'd up by the
  finished job, return the job_number of the job that should be scheduled to
  run on core core_id.
 
  @param core_id the zero-based index of the core where the job was located.
  @param job_number a globally unique identification number of the job.
  @param time the current time of the simulator.
  @return job_number of the job that should be scheduled to run on core core_id
  @return -1 if core should remain idle.
 */
int scheduler_job_finished(int core_id, int job_number, int time)
{
  // Add this job's waiting time to the avg waiting time
  m_waitingTime += time - m_coreArr[core_id]->arrivalTime - m_coreArr[core_id]->originalProcessTime;
  m_turnaroundTime += time - m_coreArr[core_id]->arrivalTime;
  m_responseTime += m_coreArr[core_id]->responseTime;
  m_numJobs++;

  // Free up the core where the finished job has completed
  free(m_coreArr[core_id]);
  m_coreArr[core_id] = NULL;

  if (priqueue_size(&q) != 0)
  {
    job_t* temp = (job_t*)priqueue_poll(&q);
    if (m_type == PSJF)
    {
      temp->lastCheckedTime = time;
    }
    m_coreArr[core_id] = temp;
    if(m_coreArr[core_id]->responseTime == -1)
    {
      m_coreArr[core_id]->responseTime = time - m_coreArr[core_id]->arrivalTime;
    }

    return temp->pid;
  }
  return -1;
}


/**
  When the scheme is set to RR, called when the quantum timer has expired
  on a core.
 
  If any job should be scheduled to run on the core free'd up by
  the quantum expiration, return the job_number of the job that should be
  scheduled to run on core core_id.

  @param core_id the zero-based index of the core where the quantum has expired.
  @param time the current time of the simulator. 
  @return job_number of the job that should be scheduled on core cord_id
  @return -1 if core should remain idle
 */
int scheduler_quantum_expired(int core_id, int time)
{
  job_t* jobCurrentlyOnSpecifiedCore = m_coreArr[core_id];

  if (jobCurrentlyOnSpecifiedCore == NULL)
  {
    if (priqueue_size(&q) == 0)
    {
      // Core remains idle
      return -1;
    }
  }
  else
  {
    priqueue_offer(&q, jobCurrentlyOnSpecifiedCore);
  }

  m_coreArr[core_id] = priqueue_poll(&q);
  if(m_coreArr[core_id]->responseTime == -1)
  {
    m_coreArr[core_id]->responseTime = time - m_coreArr[core_id]->arrivalTime;
  }
  return m_coreArr[core_id]->pid;
}


/**
  Returns the average waiting time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all jobs that have arrived will have finished and no new jobs will arrive).
  @return the average waiting time of all jobs scheduled.
 */
float scheduler_average_waiting_time()
{
  return m_waitingTime / m_numJobs;
}


/**
  Returns the average turnaround time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all jobs that have arrived will have finished and no new jobs will arrive).
  @return the average turnaround time of all jobs scheduled.
 */
float scheduler_average_turnaround_time()
{
  return m_turnaroundTime / m_numJobs;
}


/**
  Returns the average response time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all jobs that have arrived will have finished and no new jobs will arrive).
  @return the average response time of all jobs scheduled.
 */
float scheduler_average_response_time()
{
  return m_responseTime / m_numJobs;
}


/**
  Free any memory associated with your scheduler.
 
  Assumptions:
    - This function will be the last function called in your library.
*/
void scheduler_clean_up()
{
  int i;
  for (i = 0; i < m_cores; i++)
  {
    if (m_coreArr[i] != NULL)
    {
      free(m_coreArr[i]);
    }
  }
  free(m_coreArr);
}


/**
  This function may print out any debugging information you choose. This
  function will be called by the simulator after every call the simulator
  makes to your scheduler.
  In our provided output, we have implemented this function to list the jobs in the order they are to be scheduled. Furthermore, we have also listed the current state of the job (either running on a given core or idle). For example, if we have a non-preemptive algorithm and job(id=4) has began running, job(id=2) arrives with a higher priority, and job(id=1) arrives with a lower priority, the output in our sample output will be:

    2(-1) 4(0) 1(-1)  
  
  This function is not required and will not be graded. You may leave it
  blank if you do not find it useful.
 */
void scheduler_show_queue()
{
  priqueue_print(&q);
}
