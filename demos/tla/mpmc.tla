-------------------------------- MODULE mpmc --------------------------------
EXTENDS Naturals, FiniteSets

CONSTANT BufferCapacity
CONSTANTS Readers, Writers

ASSUME Readers /= {}
ASSUME Writers /= {}
ASSUME (BufferCapacity \in Nat) /\ (BufferCapacity > 0)

VARIABLES bufferLen, threadStates

Threads == Readers \cup Writers

ThreadState == {"R", "M", "S"}
threadStates_TypeInvariant == threadStates \in [Threads -> ThreadState]

Init ==
  /\ bufferLen = 0
  /\ threadStates_TypeInvariant
  /\ \A t \in Threads: threadStates[t] = "R"

UnlockAndWakeOne(self) ==
  LET
    sleepingThreads == {t \in Threads: threadStates[t] = "S"}
  IN
    IF sleepingThreads = {}
    THEN
      threadStates' = [threadStates EXCEPT ![self] = "R"]
    ELSE
      \E t \in sleepingThreads: threadStates' = [threadStates EXCEPT ![self] = "R", ![t] = "R"]

AcquireLock(t) ==
  /\ threadStates[t] = "R"
  /\ \forall ta \in Threads: threadStates[ta] /= "M"
  /\ threadStates' = [threadStates EXCEPT ![t] = "M"]
  /\ UNCHANGED bufferLen

WriteOk(t) ==
  /\ t \in Writers
  /\ threadStates[t] = "M"
  /\ bufferLen < BufferCapacity
  /\ bufferLen' = bufferLen + 1
  /\ UnlockAndWakeOne(t) 

WriteBlock(t) ==
  /\ t \in Writers
  /\ threadStates[t] = "M"
  /\ bufferLen = BufferCapacity
  /\ threadStates' = [threadStates EXCEPT ![t] = "S"]
  /\ UNCHANGED bufferLen

ReadOk(t) ==
  /\ t \in Readers
  /\ threadStates[t] = "M"
  /\ bufferLen > 0
  /\ bufferLen' = bufferLen - 1
  /\ UnlockAndWakeOne(t)

ReadBlock(t) ==
  /\ t \in Readers
  /\ threadStates[t] = "M"
  /\ bufferLen = 0
  /\ threadStates' = [threadStates EXCEPT ![t] = "S"]
  /\ UNCHANGED bufferLen

Next == \E t \in Threads:
  \/ AcquireLock(t)
  \/ WriteOk(t)
  \/ WriteBlock(t)
  \/ ReadOk(t)
  \/ ReadBlock(t)

Spec == Init /\ [][Next]_<<bufferLen, threadStates>>

-----------------------------------------------------------------------------
AtMostOneMutexOwner ==
  LET owners == {t \in Threads: threadStates[t] = "M"}
  IN Cardinality(owners) <= 1

Liveness ==
  LET runnable == {t \in Threads: threadStates[t] \in {"R", "M"}}
  IN runnable /= {}

THEOREM Spec => []threadStates_TypeInvariant
THEOREM Spec => []AtMostOneMutexOwner
THEOREM Spec => []Liveness
=============================================================================
