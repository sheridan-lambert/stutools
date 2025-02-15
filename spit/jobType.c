#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <pthread.h>

#include <math.h>
#include <limits.h>

#include "jobType.h"
#include "utils.h"
#include "list.h"
#include "latency.h"
#include "aioRequests.h"
#include "blockVerify.h"

extern volatile int keepRunning;
extern int verbose;

#define INF_SECONDS 100000000

void jobInit(jobType *job)
{
  job->count = 0;
  job->strings = NULL;
  job->devices = NULL;
  job->deviceid = NULL;
  job->suggestedNUMA = NULL;
  job->delay = NULL;
}

void jobAddBoth(jobType *job, char *device, char *jobstring, int suggestedNUMA)
{
  //  fprintf(stderr,"add %s, %s, %d\n", device, jobstring, suggestedNUMA);
  job->strings = realloc(job->strings, (job->count+1) * sizeof(char*));
  job->devices = realloc(job->devices, (job->count+1) * sizeof(char*));
  job->deviceid = realloc(job->deviceid, (job->count+1) * sizeof(int));
  job->suggestedNUMA = realloc(job->suggestedNUMA, (job->count+1) * sizeof(int));
  job->delay = realloc(job->delay, (job->count+1) * sizeof(double));
  job->strings[job->count] = strdup(jobstring);
  job->devices[job->count] = strdup(device);
  int deviceid = job->count;
  for (int i = 0; i < job->count; i++) {
    if (strcmp(device, job->devices[i])==0) {
      deviceid = i;
      break;
    }
  }
  job->deviceid[job->count] = deviceid;
  job->delay[job->count] = 0;
  job->suggestedNUMA[job->count] = suggestedNUMA;
  job->count++;
}

void jobAdd3(jobType *job, const char *jobstring, const double delay)
{
  //  fprintf(stderr,"add3() %s %d\n", jobstring, -1);
  job->strings = realloc(job->strings, (job->count+1) * sizeof(char*));
  job->devices = realloc(job->devices, (job->count+1) * sizeof(char*));
  job->deviceid = realloc(job->deviceid, (job->count+1) * sizeof(int));
  job->suggestedNUMA = realloc(job->suggestedNUMA, (job->count+1) * sizeof(int));
  job->delay = realloc(job->delay, (job->count+1) * sizeof(double));
  job->strings[job->count] = strdup(jobstring);
  job->devices[job->count] = NULL;
  job->deviceid[job->count] = 0;
  job->delay[job->count] = delay;
  job->suggestedNUMA[job->count] = -1;
  job->count++;
}


void jobAdd(jobType *job, const char *jobstring)
{
  jobAdd3(job, jobstring, 0);
}


void jobAddExec(jobType *job, const char *jobstring, const double delay)
{
  jobAdd3(job, jobstring, delay);
}

void jobMultiply(jobType *retjobs, jobType *job, deviceDetails *deviceList, size_t deviceCount)
{
  const int origcount = job->count;
  for (int i = 0; i < origcount; i++) {
    for (size_t n = 0; n < deviceCount; n++) {
      //      fprintf(stderr,"adding...\n");
      jobAddBoth(retjobs, deviceList[n].devicename, job->strings[i], deviceList[n].numa);
    }
  }
}



void jobFileSequence(jobType *job)
{
  const int origcount = job->count;
  for (int i = 0; i < origcount; i++) {
    char s[1000];
    sprintf(s, "%s.%04d", job->devices[i], i+1);
    free(job->devices[i]);
    job->devices[i] = strdup(s);
    job->deviceid[i] = i;
  }
}


void jobDump(jobType *job)
{
  fprintf(stderr,"*info* jobDump: %zd\n", jobCount(job));
  for (int i = 0; i < job->count; i++) {
    fprintf(stderr,"*  info* job %d, device %s, deviceid %d, delay %.g, string %s, NUMA %d\n", i, job->devices[i], job->deviceid[i], job->delay[i], job->strings[i], job->suggestedNUMA[i]);
  }
}

void jobDumpAll(jobType *job)
{
  for (int i = 0; i < job->count; i++) {
    fprintf(stderr,"*info* job %d, string %s\n", i, job->strings[i]);
  }
}

void jobFree(jobType *job)
{
  for (int i = 0; i < job->count; i++) {
    //    fprintf(stderr,"freeinging %d of %d\n", i, job->count);
    if (job->strings[i]) {
      free(job->strings[i]);
      job->strings[i] = NULL;
    }
    if (job->devices[i]) {
      free(job->devices[i]);
      job->devices[i] = NULL;
    }
  }
  free(job->strings);
  job->strings = NULL;
  free(job->devices);
  job->devices = NULL;
  free(job->deviceid);
  job->deviceid = NULL;
  free(job->suggestedNUMA);
  job->suggestedNUMA = NULL;
  free(job->delay);
  job->delay = NULL;
  jobInit(job);
}

typedef struct {
  size_t id;
  positionContainer pos;
  size_t minbdSize;
  size_t maxbdSize;
  size_t minSizeInBytes;
  size_t maxSizeInBytes;
  double waitfor;
  size_t prewait;
  size_t exec;
  char *jobstring;
  char *jobdevice;
  int jobdeviceid;
  int jobnuma;
  size_t uniqueSeeds;
  size_t verifyUnique;
  size_t dumpPos;
  size_t blockSize;
  size_t highBlockSize;
  size_t queueDepth;
  long metaData;
  size_t QDbarrier;
  size_t flushEvery;
  probType rw;
  size_t startingBlock;
  size_t mod;
  size_t remain;
  int rerandomize;
  int runonce;
  int addBlockSize;
  size_t positionLimit;
  size_t LBAtimes;
  unsigned short seed;
  size_t iopstarget;
  size_t iopsdecrease;
  char *randomBuffer;
  size_t numThreads;
  size_t waitForThreads;
  size_t *go;
  size_t *go_finished;
  size_t youcanstart;
  pthread_mutex_t *gomutex;
  positionContainer **allPC;
  char *benchmarkName;
  size_t anywrites;
  size_t UUID;
  size_t seqFiles;
  size_t seqFilesMaxSizeBytes;
  FILE *fp;
  size_t jumbleRun;
  double runSeconds; // the time for a round
  double finishSeconds; // the overall thread finish time
  size_t ignoreResults;
  size_t exitIOPS;
  double timeperline;
  double ignorefirst;
  char *mysqloptions;
  char *mysqloptions2;
  char *commandstring;
  char *filePrefix;
  size_t o_direct;
  double fourkEveryMiB;
  size_t jumpK;
  lengthsType len;
  size_t firstPPositions;
  int performPreDiscard;
  int notexclusive;
  size_t posIncrement;
  int jmodonly;

  // results
  double result_writeIOPS;
  double result_readIOPS;
  double result_writeMBps;
  double result_readMBps;
  double result_tm;
} threadInfoType;


static void *runThread(void *arg)
{
  threadInfoType *threadContext = (threadInfoType*)arg;
  if (verbose >= 2) {
    fprintf(stderr,"*info* thread[%zd] job is '%s', size = %zd\n", threadContext->id, threadContext->jobstring, threadContext->pos.sz);
  }

  // allocate the position array space
  // create the positions and the r/w status
  //    threadContext->seqFiles = seqFiles;
  //    threadContext->seqFilesMaxSizeBytes = seqFilesMaxSizeBytes;
  positionContainerCreatePositions(&threadContext->pos, threadContext->jobdeviceid, threadContext->seqFiles, threadContext->seqFilesMaxSizeBytes, threadContext->rw, &threadContext->len, MIN(4096,threadContext->blockSize), threadContext->startingBlock, threadContext->minbdSize, threadContext->maxbdSize, threadContext->seed, threadContext->mod, threadContext->remain, threadContext->fourkEveryMiB, threadContext->jumpK, threadContext->firstPPositions);

  for (size_t e = 0; e < threadContext->pos.sz; e++) {
    assert(threadContext->pos.positions[e].len > 0);
  }
  

  if (verbose >= 2) {
    positionContainerCheck(&threadContext->pos, threadContext->minbdSize, threadContext->maxbdSize, threadContext->metaData ? 0 : 1 /*don't exit if meta*/);
  }

  if (threadContext->seqFiles == 0 || threadContext->firstPPositions) positionContainerRandomize(&threadContext->pos, threadContext->seed);

  if (threadContext->jumbleRun) positionContainerJumble(&threadContext->pos, threadContext->jumbleRun, threadContext->seed);

  if (threadContext->jmodonly) positionContainerModOnly(&threadContext->pos, threadContext->jmodonly, threadContext->id);

  //      positionPrintMinMax(threadContext->pos.positions, threadContext->pos.sz, threadContext->minbdSize, threadContext->maxbdSize, threadContext->minSizeInBytes, threadContext->maxSizeInBytes);
  threadContext->anywrites = (threadContext->rw.wprob > 0) || (threadContext->rw.tprob > 0);
  //  calcLBA(&threadContext->pos); // calc LBA coverage


  if (threadContext->uniqueSeeds) {
    positionContainerUniqueSeeds(&threadContext->pos, threadContext->seed, threadContext->verifyUnique);
  } else if (threadContext->metaData) {
    positionContainerAddMetadataChecks(&threadContext->pos, threadContext->metaData);
  }

  if (threadContext->iopstarget) {
    positionContainerAddDelay(&threadContext->pos, threadContext->iopstarget, threadContext->id, threadContext->iopsdecrease);
  }

  if (threadContext->dumpPos /* && !iRandom*/) {
    positionContainerDump(&threadContext->pos, threadContext->dumpPos);
  }


  CALLOC(threadContext->randomBuffer, threadContext->highBlockSize, 1);
  memset(threadContext->randomBuffer, 0, threadContext->highBlockSize);
  generateRandomBufferCyclic(threadContext->randomBuffer, threadContext->highBlockSize, threadContext->seed, threadContext->highBlockSize);

  size_t ios = 0, shouldReadBytes = 0, shouldWriteBytes = 0;
  int fd = 0,  direct = O_DIRECT;
  if (strchr(threadContext->jobstring, 'D')) {
    fprintf(stderr,"*info* thread[%zd] turning off O_DIRECT\n", threadContext->id);
    direct = 0; // don't use O_DIRECT if the user specifes 'D'
  }
  threadContext->o_direct = direct;

  char *suffix = getSuffix(threadContext->jobdevice);
  size_t logbs = 4096;
  // check block sizes
  {
    size_t phybs, max_io_bytes;
    getPhyLogSizes(suffix, &phybs, &max_io_bytes, &logbs);

    for (int i = 0; i < (int)threadContext->pos.sz; i++) {
      if (threadContext->highBlockSize > max_io_bytes) {
	if (i==0) 
	  fprintf(stderr,"*warning* device '%s' will split I/O size of %.0lf KiB [max_sectors_kb %.0lf KiB]\n", threadContext->jobdevice, TOKiB(threadContext->highBlockSize), TOKiB(max_io_bytes));
        break;
      }
    }
  }


  if (threadContext->anywrites) {
    if (threadContext->filePrefix) {
      // always create new file on prefix and writes, delete existing
      int tr = 0;
      if (threadContext->seqFiles != 0) {
	tr = O_TRUNC; // if seq then create the file first
      } else {
	// if random create file
	createFile(threadContext->jobdevice, threadContext->maxSizeInBytes);
      }
      fd = open(threadContext->jobdevice, O_RDWR | O_CREAT | tr | direct, S_IRUSR | S_IWUSR);
      if (verbose) fprintf(stderr,"*info* open with O_RDWR, direct=%d, no excl specified\n", direct);
    } else {
      // else just open
      int excl = (threadContext->id == 0) ? O_EXCL : 0;
      if (threadContext->notexclusive) excl = 0;
      if ((excl == 0) && (threadContext->id == 0)) {
        fprintf(stderr,"*warning* open device FOR WRITES without O_EXCL! (WARNING verification can't guarantee exclusive)\n");
      }
      fd = open(threadContext->jobdevice, O_RDWR | direct | excl);
      if (verbose) fprintf(stderr,"*info* open with O_RDWR, direct=%d, excl=%d\n", direct, excl);
    }
  } else {
    int excl = (threadContext->id == 0) ? O_EXCL : 0;
    if (threadContext->notexclusive) excl = 0;
    if ((excl == 0) && (threadContext->id == 0)) {
      fprintf(stderr,"*info* open device without O_EXCL\n");
    }
    fd = open(threadContext->jobdevice, O_RDONLY | direct | excl);
    if (verbose >= 2) fprintf(stderr,"*info* open with O_RDONLY\n");
  }



  if (fd < 0) {
    fprintf(stderr,"*error* problem opening '%s' with direct=%d, writes=%zd\n", threadContext->jobdevice, direct, threadContext->anywrites);
    fprintf(stderr,"*info* some filesystems don't support direct, maybe add D to the command string.\n");
    perror(threadContext->jobdevice);
    exit(-1);
  }



//    fprintf(stderr,"*info* device %s, physical io size %zd, logical io size %zd\n", threadContext->jobdevice, phybs, logbs);

  if (verbose) {
    char *model = getModel(suffix);
    if (model && threadContext->id == 0) {
      fprintf(stderr,"*info* device: '%s'\n", model);
      free(model);
    }
  }
  const int numRequests = getNumRequests(suffix);
  if (threadContext->id==0) {
    fprintf(stderr,"*info* queue/nr_requests for '%s' = %d %s\n", threadContext->jobdevice, numRequests, numRequests<128 ? "WARNING":"");
  }
  if (getWriteCache(suffix) > 0) {
    fprintf(stderr,"*****************\n");
    fprintf(stderr,"* w a r n i n g * device %s is configured with volatile cache\n", threadContext->jobdevice);
    fprintf(stderr,"* The following results are not ENTERPRISE certified. \n");
    fprintf(stderr,"* /sys/block/%s/queue/write_cache should be 'write through'.\n", suffix);
    //    fprintf(stderr,"* Results file has been disabled.\n");
    //    threadContext->ignoreResults = 1;
    fprintf(stderr,"*****************\n");
  }

  if (!threadContext->exec && (threadContext->finishSeconds < threadContext->runSeconds)) {
    fprintf(stderr,"*warning* timing %.1lf > %.1lf doesn't make sense\n", threadContext->runSeconds, threadContext->finishSeconds);
  }
  //  double localrange = TOGB(threadContext->maxbdSize - threadContext->minbdSize);
  // minSizeInBytes is the -G range. If there are multiple threads carving the LBA up, it'll be specified as the min/max bdsize
  size_t outerrange = threadContext->maxbdSize - threadContext->minbdSize;
  size_t sumrange = 0;
  for (int i = 0; i < (int)threadContext->pos.sz; i++) {
    sumrange += threadContext->pos.positions[i].len;
  }
  if ((threadContext->id == 0) && (sumrange < outerrange*0.99)) {
    fprintf(stderr,"*warning* the range covered (%zd positions covering %.3lf GiB) is < 99%% of available range (%.3lf GiB)\n", threadContext->pos.sz, TOGiB(sumrange), TOGiB(outerrange));
  }
  if (verbose >= 1)
    fprintf(stderr,"*info* [t%zd] '%s %s' s%zd (%.0lf KiB), [%zd] (LBA %.0lf%%, [%.2lf,%.2lf]/%.2lf GiB), n=%d, q%zd (Q%zd) r/w/t=%.1g/%.1g/%.1g, F=%zd, k=[%.0lf-%.0lf], R=%u, B%zd W%.1lf T%.1lf t%.1lf x%zd X%zd\n", threadContext->id, threadContext->jobstring, threadContext->jobdevice, threadContext->seqFiles, TOKiB(threadContext->seqFilesMaxSizeBytes), threadContext->pos.sz, sumrange * 100.0 / outerrange, TOGiB(threadContext->minbdSize), TOGiB(threadContext->maxbdSize), TOGiB(outerrange), threadContext->rerandomize, threadContext->queueDepth, threadContext->QDbarrier, threadContext->rw.rprob, threadContext->rw.wprob, threadContext->rw.tprob, threadContext->flushEvery, TOKiB(threadContext->blockSize), TOKiB(threadContext->highBlockSize), threadContext->seed, threadContext->prewait, threadContext->waitfor, threadContext->runSeconds, threadContext->finishSeconds, threadContext->LBAtimes, threadContext->positionLimit);


  // do the mahi
  pthread_mutex_lock(threadContext->gomutex);
  (*threadContext->go)++;
  pthread_mutex_unlock(threadContext->gomutex);

  while(*threadContext->go < threadContext->waitForThreads) {
    usleep(10);
  }
  if (verbose >= 2) {
    fprintf(stderr,"*info* starting thread %zd ('%s')\n", threadContext->id, threadContext->jobstring);
  }


  double starttime = timedouble();
  if (threadContext->prewait) {
    fprintf(stderr,"*info* sleeping for %zd\n", threadContext->prewait);
    sleep(threadContext->prewait);
  }

  if (threadContext->exec) {
    char *comma = strchr(threadContext->jobstring, ',');
    if (comma && *(comma+1)) {
      char *env[] = {"/bin/bash", "-c", comma+1, NULL};
      fprintf(stderr,"*info* executing '%s'\n", threadContext->jobstring);
      runCommand("/bin/bash", env);
      return NULL;
    }
  }



  size_t iteratorCount = 0, iteratorMax = 0;

  size_t roundByteLimit = 0, totalByteLimit = 0, posLimit = 0, totalPosLimit = 0; // set two of the limits to 0
  // the other limit is threadContext->runSeconds
  float timeLimit = threadContext->runSeconds;
  size_t doRounds = 0; // don't do rounds
  
  if (threadContext->rerandomize || threadContext->addBlockSize || threadContext->runonce) {
    // n or N option
    // one of the three limits: time, positions or bytes
    // if we have a timelimit let's use that
    if (threadContext->LBAtimes) {
      iteratorMax = threadContext->LBAtimes;
      roundByteLimit = outerrange ; // if x2 n  
      totalByteLimit = roundByteLimit * iteratorMax;
      timeLimit = INF_SECONDS;
      doRounds = 1; // but do if nN with x
    } else if (threadContext->positionLimit || threadContext->runonce) {
      posLimit = threadContext->pos.sz;
      totalPosLimit = posLimit * iteratorMax;
      timeLimit = INF_SECONDS;
      if (threadContext->runonce) {
	iteratorMax = 0; // don't do rounds with O
      } else {
	iteratorMax = threadContext->positionLimit;
	doRounds = 1; // but do if nN with X
      }
    }
  } else if (threadContext->LBAtimes) {
    // specifing an x option
    // if we specify xn
    roundByteLimit = outerrange * threadContext->LBAtimes;
  } else if (threadContext->positionLimit) {
    posLimit = threadContext->pos.sz * threadContext->positionLimit;
  }

  if (threadContext->posIncrement) {
    if (posLimit) {
      posLimit = posLimit / threadContext->posIncrement;
    } else {
      posLimit = threadContext->pos.sz / threadContext->posIncrement;
    }
    fprintf(stderr,"*info* posLimit = %zd\n", posLimit);
  }

  //  if (threadContext->performPreDiscard) {
  //    iteratorMax = threadContext->LBAtimes;
  //  }


  if (threadContext->id == 0) {
    //    if (verbose) {
    fprintf(stderr,"*info* doRounds %zd, rounds %zd, timeLimit %.1lf, posLimit %zd, roundByteLimitPerIteration %zd (%.03lf GiB)\n", doRounds, iteratorMax, timeLimit, posLimit, roundByteLimit, TOGiB(roundByteLimit));
    fflush(stderr);
      //    }
  }


  size_t discard_max_bytes, discard_granularity, discard_zeroes_data, alignment_offset;
  getDiscardInfo(suffix, &alignment_offset, &discard_max_bytes, &discard_granularity, &discard_zeroes_data);
  if (verbose) {
    fprintf(stderr,"*info* discardInfo: alignment_offset %zd / max %zd / granularity %zd / zeroes_data %zd\n", alignment_offset, discard_max_bytes, discard_granularity, discard_zeroes_data);
  }

  if (threadContext->rw.tprob > 0 || threadContext->performPreDiscard) {
    if (discard_max_bytes == 0) {
      fprintf(stderr,"*error* TRIM/DISCARD is not supported and it has been specified\n");
    }
  }


  size_t totalB = 0, ioerrors = 0, totalP = 0;
  
  while (keepRunning) {

    if (threadContext->performPreDiscard) {
      if (threadContext->anywrites && discard_max_bytes) {
        performDiscard(fd, threadContext->jobdevice, threadContext->minbdSize, threadContext->maxbdSize, discard_max_bytes, discard_granularity, NULL, 0, 0);
      }
    }

    if (verbose) fprintf(stderr,"*iteration* %zd\n", iteratorCount);

    totalB += aioMultiplePositions(&threadContext->pos, threadContext->pos.sz, timedouble() + timeLimit, roundByteLimit, threadContext->queueDepth, -1 /* verbose */, 0, MIN(logbs, threadContext->blockSize), &ios, &shouldReadBytes, &shouldWriteBytes, posLimit , 1, fd, threadContext->flushEvery, &ioerrors, threadContext->QDbarrier, discard_max_bytes, threadContext->fp, threadContext->jobdevice, threadContext->posIncrement);
    totalP += posLimit;

    if (!doRounds) break;

    // check exit constraints
    if (roundByteLimit && (totalB >= totalByteLimit)) {
      if (verbose) fprintf(stderr,"*info* reached byte limit of %zd\n", totalByteLimit);
      break;
    }
    if (totalPosLimit && (totalP >= totalPosLimit)) {
      if (verbose) fprintf(stderr,"*info* reached pos limit of %zd\n", totalPosLimit);
    }

    if (threadContext->finishSeconds && (timedouble() > starttime + threadContext->finishSeconds)) {
      if (verbose) fprintf(stderr,"*info* timer threshold reached\n");
      break;
    }
    
    iteratorCount++;
    if (verbose) fprintf(stderr,"*info* finished pass %zd of %zd\n", iteratorCount, iteratorMax);
    
    if (verbose >= 1) {
      if (!keepRunning && threadContext->id == 0) {
        fprintf(stderr,"*info* interrupted...\n");
      }
    }
    if (keepRunning && doRounds && (iteratorCount < iteratorMax)) {
      if (threadContext->rerandomize) {
        fprintf(stderr,"*info* shuffling positions, 1st = %zd\n", threadContext->pos.positions[0].pos);
        positionContainerRandomize(&threadContext->pos, threadContext->seed + iteratorCount);
      }
      if (threadContext->addBlockSize) {
        fprintf(stderr,"*info* adding %zd to all positions, 1st = %zd\n", threadContext->highBlockSize, threadContext->pos.positions[0].pos);
        positionAddBlockSize(threadContext->pos.positions, threadContext->pos.sz, threadContext->highBlockSize, threadContext->minbdSize, threadContext->maxbdSize);
      }
    }
    usleep(threadContext->waitfor * 1000.0 * 1000); // micro seconds

    if ((iteratorMax > 0) && (iteratorCount >= iteratorMax)) {
      break;
    }
  }
  if (suffix) free(suffix);


  if (verbose >= 2) {
    fprintf(stderr,"*info [thread %zd] finished '%s'\n", threadContext->id, threadContext->jobstring);
  }
  threadContext->pos.elapsedTime = timedouble() - starttime;

  pthread_mutex_lock(threadContext->gomutex);
  (*threadContext->go_finished)++;
  pthread_mutex_unlock(threadContext->gomutex);

  //  if (verbose) {fprintf(stderr,"*info* starting fdatasync()..."); fflush(stderr);}
  //  fdatasync(fd); // make sure all the data is on disk before we axe off the ioc
  //  if (verbose) {fprintf(stderr," finished\n"); fflush(stderr);}

  close(fd);

  if (ioerrors) {
    fprintf(stderr,"*warning* there were %zd IO errors\n", ioerrors);
    // exit(-1);
  }

  return NULL;
}


char *getValue(const char *os, const char *prefix)
{
  assert(prefix);
  if (!os) {
    //    abort();
    return strdup("NULL");
  }

  const int plen = strlen(prefix);

  char *i=strstr(os, prefix);
  if (!i) {
    return strdup("NULL");
  }
  char *k=strchr(i, ',');
  if (!k) {
    return strdup(i + plen);
  } else {
    char s[1000];
    strncpy(s, i + plen, k - i - plen);
    s[k-i-plen]=0;
    return strdup(s);
  }
}

static void *runThreadTimer(void *arg)
{
  threadInfoType *threadContext = (threadInfoType*)arg;
  double TIMEPERLINE = threadContext->timeperline;
  //  if (TIMEPERLINE < 0.000001) TIMEPERLINE=0.000001;
  if (TIMEPERLINE <= 0) TIMEPERLINE = 0.00001;
  double ignorefirst = threadContext->ignorefirst;
  if (ignorefirst < 0) ignorefirst = 0;

  if (threadContext->finishSeconds == -1) { // run forever
    if (verbose) fprintf(stderr,"*info* defaulting timer to 10 years\n");
    threadContext->finishSeconds = 86400 * 365 * 10;
  }

  while(*threadContext->go < threadContext->waitForThreads) {
    usleep(100);
    //    fprintf(stderr,"."); fflush(stderr);
  }
  //  fprintf(stderr,"%zd %zd\n", *threadContext->go, threadContext->waitForThreads);

  if (verbose >= 2) {
    fprintf(stderr,"*info* timer thread, threads %zd, %.2lf per line, ignore first %.2lf seconds, %s, finish time %.1lf\n", threadContext->numThreads, TIMEPERLINE, ignorefirst, threadContext->benchmarkName, threadContext->finishSeconds);
  }

  double thistime = 0;
  volatile size_t last_trb = 0, last_twb = 0, last_tri = 0, last_twi = 0;
  volatile size_t trb = 0, twb = 0, tri = 0, twi = 0;


  FILE *fp = NULL, *fpmysql = NULL;
  char s[1000];
  if (threadContext->benchmarkName) {
    // first write out .gnufile
    sprintf(s, "%s_r.gnu", threadContext->benchmarkName);
    fp = fopen(s, "wt");
    if (fp) {
      fprintf(fp, "set title 'Speed over time: read (confidence intervals)'\n");
      fprintf(fp, "set key autotitle columnhead\n");
      fprintf(fp, "set xlabel 'seconds'\n");
      fprintf(fp, "set ylabel 'MB/s'\n");
      fprintf(fp, "set key outside top center horiz\n");
      fprintf(fp, "ymin=100000; ymax = 0;\n");
      fprintf(fp, "do for [i=1:9] {\n");
      fprintf(fp, "stats '<~/stutools/utils/dist -c 3 <out' u i nooutput\n");
      fprintf(fp, "if (STATS_min < ymin) {ymin=STATS_min}\n");
      fprintf(fp, "if (STATS_max > ymax) {ymax=STATS_max}\n");
      fprintf(fp, "}\n");
      fprintf(fp, "set yrange [ymin:ymax]\n");
      fprintf(fp, "plot for [i=10:2:-1] '<~/stutools/utils/dist -p 1 -c 3 <out' using 1:i with filledcurves x1 linestyle i title columnheader(i)\n");
    }

    sprintf(s, "%s_w.gnu", threadContext->benchmarkName);
    fp = fopen(s, "wt");
    if (fp) {
      fprintf(fp, "set title 'Speed over time: write (confidence intervals)'\n");
      fprintf(fp, "set key autotitle columnhead\n");
      fprintf(fp, "set xlabel 'seconds'\n");
      fprintf(fp, "set ylabel 'MB/s'\n");
      fprintf(fp, "set key outside top center horiz\n");
      fprintf(fp, "ymin=100000; ymax = 0;\n");
      fprintf(fp, "do for [i=1:9] {\n");
      fprintf(fp, "stats '<~/stutools/utils/dist -c 5 <out' u i nooutput\n");
      fprintf(fp, "if (STATS_min < ymin) {ymin=STATS_min}\n");
      fprintf(fp, "if (STATS_max > ymax) {ymax=STATS_max}\n");
      fprintf(fp, "}\n");
      fprintf(fp, "set yrange [ymin:ymax]\n");
      fprintf(fp, "plot for [i=10:2:-1] '<~/stutools/utils/dist -p 1 -c 5 <out' using 1:i with filledcurves x1 linestyle i title columnheader(i)\n");
    }

    // then mysql
    sprintf(s, "%s.my", threadContext->benchmarkName);
    fp = fopen(threadContext->benchmarkName, "wt");
    if (threadContext->mysqloptions) {
      fpmysql = fopen(s, "wt");
      if (!fpmysql) {
        perror(s);
      } else {
        fprintf(fpmysql, "# start of file\n");
        fflush(fpmysql);
      }
    }

    if (fp) {
      fprintf(stderr,"*info* benchmark file '%s' (MySQL '%s.my', gnuplot '%s.gnu')\n", threadContext->benchmarkName, threadContext->benchmarkName, threadContext->benchmarkName);
    } else {
      fprintf(stderr,"*error* couldn't create benchmark file '%s' (MySQL '%s')\n", threadContext->benchmarkName, s);
      perror(threadContext->benchmarkName);
    }
  }

  double total_printed_r_bytes = 0, total_printed_w_bytes = 0, total_printed_r_iops = 0, total_printed_w_iops = 0;
  size_t exitcount = 0;
  double starttime = timedouble();
  double lasttime = starttime, nicetime = TIMEPERLINE;
  double lastprintedtime = lasttime;

  // grab the start, setup the "last" values
  for (size_t j = 0; j < threadContext->numThreads; j++) {
    assert(threadContext->allPC);
    if (threadContext->allPC && threadContext->allPC[j]) {
      last_tri += threadContext->allPC[j]->readIOs;
      last_trb += threadContext->allPC[j]->readBytes;

      last_twi += threadContext->allPC[j]->writtenIOs;
      last_twb += threadContext->allPC[j]->writtenBytes;
    }
  }

  double last_devicerb = 0, last_devicewb = 0;

  // loop until the time runs out
  int printlinecount = 0;
  while (keepRunning && ((thistime = timedouble()) < starttime + threadContext->finishSeconds + TIMEPERLINE)) {

    if ((thistime - starttime >= nicetime) && (thistime < starttime + threadContext->finishSeconds + TIMEPERLINE)) {
      printlinecount++;
      // find a nice time for next values
      if (threadContext->timeperline <= 0) { // if -s set to <= 0
        if (thistime - starttime < 0.1) { // 10ms for 1 s
          TIMEPERLINE = 0.001;
        } else  if (thistime - starttime < 10) { // 10ms for 1 s
          TIMEPERLINE = 0.01;
        } else if (thistime - starttime < 30) { // 10ms for up to 5 sec
          TIMEPERLINE = 0.1;
        } else {
          TIMEPERLINE = 1;
        }
      }
      nicetime = thistime - starttime + TIMEPERLINE;
      nicetime = (size_t)(floor(nicetime / TIMEPERLINE)) * TIMEPERLINE;

      trb = 0;
      twb = 0;
      tri = 0;
      twi = 0;

      double devicerb = 0, devicewb = 0, devicetimeio = 0, devicereadio = 0, devicewriteio = 0;

      size_t n_inflight_print = 10;
      const size_t cur_disk_inflight_str_len = 1024;
      char cur_disk_inflight_str[ cur_disk_inflight_str_len ];

      for (size_t j = 0; j < threadContext->numThreads; j++) {
        assert(threadContext->allPC);
        if (threadContext->allPC && threadContext->allPC[j]) {
          tri += threadContext->allPC[j]->readIOs;
          trb += threadContext->allPC[j]->readBytes;

          twi += threadContext->allPC[j]->writtenIOs;
          twb += threadContext->allPC[j]->writtenBytes;
        }
      }

      total_printed_r_bytes = trb;
      total_printed_r_iops =  tri;

      total_printed_w_bytes = twb;
      total_printed_w_iops =  twi;

      if (threadContext->pos.diskStats) {
        diskStatFinish(threadContext->pos.diskStats);
        devicerb += diskStatTBRead(threadContext->pos.diskStats);
        devicereadio = diskStatTBReadIOs(threadContext->pos.diskStats); // io
        devicewb += diskStatTBWrite(threadContext->pos.diskStats);
        devicewriteio = diskStatTBWriteIOs(threadContext->pos.diskStats); // io
        devicetimeio = diskStatTBTimeSpentIO(threadContext->pos.diskStats);

        if( verbose >= 2 ) {
          for ( size_t i = 0; i < threadContext->pos.diskStats->allocDevices; i++ ) {
            const char* delim = i == 0 ? "" : " ";
            fprintf( stderr, "%s%s(%ld)", delim, threadContext->pos.diskStats->suffixArray[i], threadContext->pos.diskStats->inflightArray[i] );
          }
          fprintf( stderr, "\n" );
        }
        diskStatMaxQDStr(threadContext->pos.diskStats, n_inflight_print, cur_disk_inflight_str, cur_disk_inflight_str_len);
        diskStatRestart(threadContext->pos.diskStats); // reset
      }

      if (twb + trb >= ignorefirst) {

        const double gaptime = thistime - lastprintedtime;

        double readB     = (trb - last_trb) / gaptime;
        double readIOPS  = (tri - last_tri) / gaptime;

        double writeB    = (twb - last_twb) / gaptime;
        double writeIOPS = (twi - last_twi) / gaptime;

        if ((tri - last_tri) || (twi - last_twi) || (devicerb - last_devicerb) || (devicewb - last_devicewb)) { // if any IOs in the period to display note the time
          lastprintedtime = thistime;
        }

        const double elapsed = thistime - starttime;

        pthread_mutex_lock(threadContext->gomutex);
        fprintf(stderr,"[%2.2lf / %zd] read ", elapsed, *threadContext->go - *threadContext->go_finished);
        pthread_mutex_unlock(threadContext->gomutex);

        //fprintf(stderr,"%5.0lf", TOMB(readB));
        commaPrint0dp(stderr, TOMB(readB));
        fprintf(stderr," MB/s (");
        //fprintf(stderr,"%7.0lf", readIOPS);
        commaPrint0dp(stderr, readIOPS);
        fprintf(stderr," IOPS / %.0lf), write ", (readIOPS == 0) ? 0 : (readB * 1.0) / (readIOPS));

        //fprintf(stderr,"%5.0lf", TOMB(writeB));
        commaPrint0dp(stderr, TOMB(writeB));
        fprintf(stderr," MB/s (");
        //fprintf(stderr,"%7.0lf", writeIOPS);
        commaPrint0dp(stderr, writeIOPS);

        fprintf(stderr," IOPS / %.0lf), total %.2lf GiB", (writeIOPS == 0) ? 0 : (writeB * 1.0) / (writeIOPS), TOGiB(trb + twb));
        if (verbose >= 1) {
          fprintf(stderr," == %zd %zd %zd %zd s=%.5lf == ", trb, tri, twb, twi, gaptime);
        }
        if (threadContext->pos.diskStats) {
          fprintf(stderr,
                  " (R %.0lf MB/s, W %.0lf MB/s, %% %.0lf, IO/s %.0lf, %s)",
                  TOMB(devicerb / gaptime), TOMB(devicewb / gaptime), 100.0 * devicetimeio / (gaptime*1000),
                  (devicereadio + devicewriteio)/gaptime, cur_disk_inflight_str);
        }
        fprintf(stderr,"\n");

        if (fp) {
          fprintf(fp, "%.4lf\t%lf\t%.1lf\t%.0lf\t%.1lf\t%.0lf\t%.1lf\t%.1lf\t%.0lf\n", elapsed, thistime, TOMB(readB), readIOPS, TOMB(writeB), writeIOPS, TOMB(devicerb / gaptime), TOMB(devicewb / gaptime), threadContext->pos.diskStats ? 100.0 * devicetimeio / (gaptime*1000) : 0);
          fflush(fp);
        }

        if (threadContext->exitIOPS && twb > threadContext->maxbdSize) {
          if (writeIOPS + readIOPS < threadContext->exitIOPS) {
            exitcount++;
            if (exitcount >= 10) {
              fprintf(stderr,"*warning* early exit after %.1lf GB due to %zd periods of low IOPS (%.0lf < % zd)\n", TOGB(twb), exitcount, writeIOPS + readIOPS, threadContext->exitIOPS);
              keepRunning = 0;
              break;
            }
          }
        }

        last_trb = trb;
        last_tri = tri;
        last_twb = twb;
        last_twi = twi;
      } // ignore first
      lasttime = thistime;
    }
    usleep(1000000*TIMEPERLINE/10); // display to 0.001 s

    if (thistime > starttime + threadContext->finishSeconds + 30) {
      fprintf(stderr,"*error* still running! watchdog termination (%.1lf > %.1lf + %.1lf\n", thistime, starttime, threadContext->finishSeconds + 30);
      keepRunning = 0;
      exit(-1);
    }
  }
  if (fp) {
    fclose(fp);
    fp = NULL;
  }

  //  fprintf(stderr,"start %.0lf last printed %.0lf this %.0lf actual %.0lf\n",0.0f, lastprintedtime - starttime, thistime - lastprintedtime, thistime - starttime);
  double tm = lastprintedtime - starttime;
  if (thistime - lastprintedtime >= 2) {
    tm = thistime - starttime; // if more than two seconds since a non zero line
  }
  fprintf(stderr,"*info* summary: read %.0lf MB/s (%.0lf IOPS), ", TOMB(total_printed_r_bytes)/tm, total_printed_r_iops/tm);
  fprintf(stderr,"write %.0lf MB/s (%.0lf IOPS)\n", TOMB(total_printed_w_bytes)/tm, total_printed_w_iops/tm);

  if (fpmysql) {
    fprintf(fpmysql, "CREATE TABLE if not exists benchmarks (id int not null auto_increment, date datetime not null, blockdevice char(30) not null, read_mbs float not null, read_iops float not null, write_mbs float not null, write_iops float not null, command char(100) not null, version char(10) not null, machine char(20) not null, iotype char(10) not null, opsize char(10) not null, iopattern char(10) not null, qd int not null, devicestate char(15) not null, threads int not null, read_total_gb float not null, write_total_gb float not null, tool char(10) not null, runtime float not null, mysqloptions char(200) not null, os char(20) not null, degraded int not null, k int not null, m int not null, checksum char(15), encryption char(10), cache int not null, unixdate int not null, machineUptime int, totalRAM bigint, precondition char(50), primary key(id));\n");
    fprintf(fpmysql, "insert into benchmarks set tool='spit', date=NOW(), unixdate='%.0lf', read_mbs='%.0lf', read_iops='%.0lf', write_mbs='%.0lf', write_iops='%.0lf'", floor(starttime), TOMB(total_printed_r_bytes)/tm, total_printed_r_iops/tm, TOMB(total_printed_w_bytes)/tm, total_printed_w_iops/tm);
    fprintf(fpmysql, ", read_total_gb='%.1lf', write_total_gb='%.1lf'", TOGB(trb), TOGB(twb));
    fprintf(fpmysql, ", threads='%zd', runtime='%.0lf'", threadContext->numThreads, ceil(thistime) - floor(starttime)); // insert a range that covers everything, including the printed values
    fprintf(fpmysql, ", mysqloptions='%s'", threadContext->mysqloptions);
    fprintf(fpmysql, ", command='%s'", threadContext->commandstring);
    fprintf(fpmysql, ", machineUptime='%zd'", getUptime());
    fprintf(fpmysql, ", totalRAM='%zd'", totalRAM());
    {
      char *os = getValue(threadContext->mysqloptions2, "os=");
      fprintf(fpmysql, ", os='%s'", os);
      if (os) free(os);
    }

    {
      char *version = getValue(threadContext->mysqloptions2, "version=");
      fprintf(fpmysql, ", version='%s'", version);
      if (version) free(version);
    }

    {
      char *value = getValue(threadContext->mysqloptions2, "machine=");
      if (strcmp(value,"NULL")==0) {
        value = hostname();
      }
      fprintf(fpmysql, ", machine='%s'", value);
      if (value) free(value);
    }

    {
      char *value = getValue(threadContext->mysqloptions2, "blockdevice=");
      fprintf(fpmysql, ", blockdevice='%s'", value);
      if (value) free(value);
    }

    {
      char *value = getValue(threadContext->mysqloptions, "iotype=");
      fprintf(fpmysql, ", iotype='%s'", value);
      if (value) free(value);
    }


    {
      char *value = getValue(threadContext->mysqloptions, "precondition=");
      fprintf(fpmysql, ", precondition='%s'", value);
      if (value) free(value);
    }


    {
      char *value = getValue(threadContext->mysqloptions, "opsize=");
      fprintf(fpmysql, ", opsize='%s'", value);
      if (value) free(value);
    }

    {
      char *value = getValue(threadContext->mysqloptions, "iopattern=");
      fprintf(fpmysql, ", iopattern='%s'", value);
      if (value) free(value);
    }


    {
      char *value = getValue(threadContext->mysqloptions, "qd=");
      fprintf(fpmysql, ", qd='%s'", value);
      if (value) free(value);
    }

    {
      char *value = getValue(threadContext->mysqloptions, "devicestate=");
      fprintf(fpmysql, ", devicestate='%s'", value);
      if (value) free(value);
    }

    {
      char *value = getValue(threadContext->mysqloptions, "degraded=");
      fprintf(fpmysql, ", degraded='%s'", value);
      if (value) free(value);
    }

    {
      char *value = getValue(threadContext->mysqloptions, "k=");
      fprintf(fpmysql, ", k='%s'", value);
      if (value) free(value);
    }

    {
      char *value = getValue(threadContext->mysqloptions, "m=");
      fprintf(fpmysql, ", m='%s'", value);
      if (value) free(value);
    }

    {
      char *value = getValue(threadContext->mysqloptions, "checksum=");
      fprintf(fpmysql, ", checksum='%s'", value);
      if (value) free(value);
    }

    {
      char *value = getValue(threadContext->mysqloptions, "encryption=");
      fprintf(fpmysql, ", encryption='%s'", value);
      if (value) free(value);
    }

    {
      char *value = getValue(threadContext->mysqloptions, "cache=");
      fprintf(fpmysql, ", cache='%s'", value);
      if (value) free(value);
    }

    fprintf(fpmysql, ";\n");
    fprintf(fpmysql, "select \"last insert id\", last_insert_id();\n");
    fclose(fpmysql);
  }




  if (verbose) fprintf(stderr,"*info* finished thread timer\n");
  //  keepRunning = 0;

  threadContext->result_writeIOPS = total_printed_w_iops /tm;
  threadContext->result_readIOPS =  total_printed_r_iops /tm;
  threadContext->result_writeMBps = TOGiB(total_printed_w_bytes) / tm;
  threadContext->result_readMBps = TOGiB(total_printed_r_bytes) /tm;
  threadContext->result_tm = tm;


  return NULL;
}



void jobRunThreads(jobType *job, const int num, char *filePrefix,
                   const size_t minSizeInBytes,
                   const size_t maxSizeInBytes,
                   const double runseconds, const size_t dumpPos, char *benchmarkName, const size_t origqd,
                   unsigned short seed, FILE *savePositions, diskStatType *d, const double timeperline, const double ignorefirst, const size_t verify,
                   char *mysqloptions, char *mysqloptions2, char *commandstring, char* numaBinding, const int performPreDiscard,
                   resultType *result, size_t ramBytesForPositions, size_t notexclusive)
{
  pthread_t *pt;
  CALLOC(pt, num+1, sizeof(pthread_t));

  threadInfoType *threadContext;
  CALLOC(threadContext, num+1, sizeof(threadInfoType));

  keepRunning = 1;

  //  if (notexclusive) {
  //    fprintf(stderr,"*warning* specifying to open devices without O_EXCL\n");
  //  }

  if (seed == 0) {
    const double d = timedouble() * 100.0; // use the fractions so up/return is different
    const unsigned long d2 = (unsigned long)d;
    seed = d2 & 0xffff;
    fprintf(stderr,"*info* setting seed based on the time to '-R %d'\n", seed);
  }

  positionContainer **allThreadsPC;
  CALLOC(allThreadsPC, num, sizeof(positionContainer*));

  const double thetime = timedouble();
  const size_t UUID = thetime * 10;


  if (verbose) {
    fprintf(stderr,"*info* low: %.1lf GiB, high: %.1lf GiB\n", TOGiB(minSizeInBytes), TOGiB(maxSizeInBytes));
  }


  size_t *go = malloc(sizeof(size_t));
  *go = 0;

  size_t *go_f = malloc(sizeof(size_t));
  *go_f = 0;

  for (int i = 0; i < num + 1; i++) { // +1 as the timer is the last onr

    threadContext[i].go = go;
    threadContext[i].jmodonly = 0;
    threadContext[i].fp = savePositions;
    threadContext[i].notexclusive = notexclusive;
    threadContext[i].go_finished = go_f;
    threadContext[i].filePrefix = filePrefix;
    threadContext[i].mysqloptions = mysqloptions;
    threadContext[i].mysqloptions2 = mysqloptions2;
    threadContext[i].commandstring = commandstring;
    threadContext[i].ignoreResults = 0;
    threadContext[i].id = i;
    threadContext[i].performPreDiscard = performPreDiscard;
    threadContext[i].runSeconds = runseconds;
    threadContext[i].LBAtimes = 0;
    threadContext[i].positionLimit = 0;
    threadContext[i].finishSeconds = runseconds;
    threadContext[i].exitIOPS = 0;
    if (i == num) {
      threadContext[i].benchmarkName = benchmarkName;
    } else {
      threadContext[i].benchmarkName = NULL;
    }
    threadContext[i].UUID = UUID;
    positionContainerInit(&threadContext[i].pos, threadContext[i].UUID);
    allThreadsPC[i] = &threadContext[i].pos;
    threadContext[i].numThreads = num;
    threadContext[i].allPC = allThreadsPC;
  }

  for (int i = 0; i < num; i++) {
    size_t localminbdsize = minSizeInBytes, localmaxbdsize = maxSizeInBytes;
    threadContext[i].runSeconds = runseconds;
    threadContext[i].finishSeconds = runseconds;
    threadContext[i].exitIOPS = 0;
    threadContext[i].jumbleRun = 0;
    threadContext[i].minSizeInBytes = minSizeInBytes;
    threadContext[i].maxSizeInBytes = maxSizeInBytes;

    int seqFiles = 1;
    size_t seqFilesMaxSizeBytes = 0;
    size_t bs = 4096, highbs = 4096;

    lengthsInit(&threadContext[i].len);

    if (verbose >= 2) {
      fprintf(stderr,"*info* setting up thread %d\n", i);
    }
    char * charBS = strchr(job->strings[i], 'M');
    if (charBS && *(charBS+1)) {

      char *endp = NULL;
      bs = 1024L * 1024 * strtod(charBS+1, &endp);
      if (bs < 512) bs = 512;
      highbs = bs;
      if (*endp == '-') {
        int nextv = atoi(endp+1);
        if (nextv > 0) {
          highbs = 1024L * 1024 * nextv;
        }
      }
    }
    if (highbs < bs) {
      highbs = bs;
    }


    int jcount = 1;
    {
      // specify the block device size in GiB
      char *charG = strchr(job->strings[i], 'j'); // j or J
      if (charG == NULL) {
	charG = strchr(job->strings[i], 'J');
      }
      
      if (charG && *(charG+1)) {
        jcount = atoi(charG+1);
        if (jcount < 1) jcount=1;
	if (*charG == 'J') {
	  //	  fprintf(stderr,"*info* jmodonly = %d\n", jcount);
	  threadContext[i].jmodonly = jcount;
	}
      }
    }
    int jindex = 0;
    {
      // specify the block device size in GiB
      char *charG = strchr(job->strings[i], '#');
      if (charG && *(charG+1)) {
        jindex = atoi(charG+1);
        if (jindex < 0) jindex=0;
      }
    }

    size_t mod = 1;
    size_t remain = 0;
    {
      // specify the block device size in GiB
      char *charG = strchr(job->strings[i], 'G');
      if (charG && *(charG+1)) {
        double lowg = 0, highg = 0;

        if ((jcount > 1) && (*(charG+1) == '_')) {
          // 2: 1/1
          lowg = minSizeInBytes + (jindex * 1.0 / (jcount)) * (maxSizeInBytes - minSizeInBytes);
          highg = minSizeInBytes + ((jindex+1) * 1.0 / (jcount)) * (maxSizeInBytes - minSizeInBytes);
        } else if ((jcount > 1) && (*(charG+1) == '%')) {
          // 2: 1/1
          mod = jcount;
          remain = i;
        } else {
          char retch = '-';
          splitRangeChar(charG + 1, &lowg, &highg, &retch);
          if (lowg > highg) {
            fprintf(stderr,"*error* low range needs to be lower [%.1lf, %.1lf]\n", lowg, highg);
            lowg = 0;
            //	  exit(1);
          }
          lowg = lowg * 1024 * 1024 * 1024;
          highg = highg * 1024 * 1024 * 1024;

          if (retch == '_') {
            size_t __lowg = (size_t)lowg + (jindex * 1.0 / (jcount)) * (size_t)(highg - lowg);
            size_t __highg = (size_t)lowg + ((jindex+1) * 1.0 / (jcount)) * (size_t)(highg - lowg);
            lowg = __lowg;
            highg = __highg;
          }

        }
        if (lowg == highg) { // if both the same, same as just having 1
          lowg = minSizeInBytes;
        }
        if (lowg < minSizeInBytes) lowg = minSizeInBytes;
        if (lowg > maxSizeInBytes) lowg = minSizeInBytes;
        if (highg > maxSizeInBytes) highg = maxSizeInBytes;
        if (highg < minSizeInBytes) highg = minSizeInBytes;
        if (highg - lowg < 4096) { // if both the same, same as just having 1
          lowg = minSizeInBytes;
          highg = maxSizeInBytes;
        }
        localminbdsize = alignedNumber(lowg, 4096);
        localmaxbdsize = alignedNumber(highg, 4096);
      }
    }

    {
      // specify the block device size in bytes
      char *charG = strchr(job->strings[i], 'b');
      if (charG && *(charG+1)) {
        double lowg = 0, highg = 0;
        splitRange(charG + 1, &lowg, &highg);
        localminbdsize = lowg;
        localmaxbdsize = highg;
        if (localminbdsize == localmaxbdsize) {
          localminbdsize = 0;
        }
        if (minSizeInBytes > maxSizeInBytes) {
          fprintf(stderr,"*error* low range needs to be lower [%.1lf, %.1lf]\n", lowg, highg);
          exit(1);
        }
      }
    }


    char *charLimit = strchr(job->strings[i], 'L');
    size_t limit = (size_t)-1;

    if (charLimit && *(charLimit+1)) {
      char *endp = NULL;
      limit = (size_t) (strtod(charLimit+1, &endp) * 1024 * 1024 * 1024);
      if (limit == 0) limit = (size_t)-1;
    }


    {
      char *charI = strchr(job->strings[i], 'I');

      if (charI && *(charI + 1)) {
        threadContext[num].exitIOPS = atoi(charI + 1);
        fprintf(stderr,"*info* exit IOPS set to %zd\n", threadContext[num].exitIOPS);
      }
    }


    {
      char *charI = strchr(job->strings[i], 'F'); // shuFfle

      if (charI && *(charI + 1)) {
        int v = atoi(charI + 1);
        if (v < 1) {
          v = 1;
        }
        threadContext[i].jumbleRun = v;
        fprintf(stderr,"*info* jumbleRun set to %zd\n", threadContext[i].jumbleRun);
      }
    }

    double fourkEveryMiB = 0;
    {
      char *sf = strchr(job->strings[i], 'a');
      if (sf && *(sf+1)) {
        fourkEveryMiB = atof(sf+1);
      }
      threadContext[i].fourkEveryMiB = fourkEveryMiB;
    }



    {
      size_t jumpK = 0; // jumpK of 0 means random else means add KiB
      char *charI = strchr(job->strings[i], 'A');

      if (charI && *(charI + 1)) {
        jumpK = atoi(charI + 1);
        if (threadContext[i].fourkEveryMiB == 0) { // if A specified then apply it always
          threadContext[i].fourkEveryMiB = 0.0001;
        }
      }
      threadContext[i].jumpK = jumpK;
      if (jumpK) {
        fprintf(stderr,"*info* jumpKiB set to %zd\n", threadContext[i].jumpK);
      }
    }

    /*    threadContext[i].gcoverhead = 0;
    {
      char *str = strchr(job->strings[i], 'o');
      if (str && *(str+1)) {
	threadContext[i].gcoverhead = atoi(str+1);
      }
      }*/

    threadContext[i].posIncrement = 0;
    {
      char *str = strchr(job->strings[i], 'K');
      if (str && *(str+1)) {
	threadContext[i].posIncrement = atoi(str+1);
	fprintf(stderr,"*info* skipping %zd actions\n", threadContext[i].posIncrement);
      }
    }

    // 'O' is really 'X1'
    threadContext[i].runonce = 0;
    {
      char *oOS = strchr(job->strings[i], 'O');
      if (oOS) {
        threadContext[i].runonce = 1;
      }
    }

    char *multLimit = strchr(job->strings[i], 'x');
    if (multLimit && *(multLimit+1)) {
      threadContext[i].LBAtimes = MAX(1, atoi(multLimit+1));
      threadContext[i].positionLimit = 0;
      threadContext[i].runSeconds = INF_SECONDS;
      threadContext[i].finishSeconds = INF_SECONDS;
      threadContext[num].finishSeconds = INF_SECONDS;
    }

    {
      char *charX = strchr(job->strings[i], 'X');

      if (charX && *(charX+1)) {
	if (threadContext[i].LBAtimes != 0) {
	  fprintf(stderr,"*error* mixing x and X options\n");
	  exit(1);
	}
        threadContext[i].positionLimit = MAX(1, atoi(charX + 1));
	threadContext[i].LBAtimes = 0;
        threadContext[i].runSeconds = INF_SECONDS;
        threadContext[i].finishSeconds = INF_SECONDS;
        threadContext[num].finishSeconds = INF_SECONDS; // the timer
      }
    }



    size_t actualfs = fileSizeFromName(job->devices[i]);
    threadContext[i].minbdSize = localminbdsize;
    threadContext[i].maxbdSize = localmaxbdsize;
    if (threadContext[i].filePrefix == NULL) {
      // if it should be there
      if (threadContext[i].maxbdSize > actualfs) {
        threadContext[i].maxbdSize = actualfs;
        fprintf(stderr,"*warning* file '%s' is smaller than specified file, shrinking to %zd\n", job->devices[i], threadContext[i].maxbdSize);
      }
    }

    charBS = strchr(job->strings[i], 'k');
    if (charBS && *(charBS+1)) {

      char *endp = NULL;
      bs = 1024 * strtod(charBS+1, &endp);
      if (bs < 512) bs = 512;
      highbs = bs;
      if (*endp == '-' || *endp == ':') {
        int nextv = atoi(endp+1);
        if (nextv > 0) {
          highbs = 1024 * nextv;
        }
      }
      if (*endp == '-')
        lengthsSetupLowHighAlignSeq(&threadContext[i].len, bs, highbs, 4096);
      else
        lengthsSetupLowHighAlignPower(&threadContext[i].len, bs, highbs, 4096);
    } else {
      lengthsAdd(&threadContext[i].len, bs, 1);
    }
    threadContext[i].blockSize = bs;
    threadContext[i].highBlockSize = highbs;


    size_t qDepth = origqd, QDbarrier = 0;

    {
      char *qdd = strchr(job->strings[i], 'q');
      if (qdd && *(qdd+1)) {
        qDepth = atoi(qdd+1);
      }
    }
    {
      char *qdd = strchr(job->strings[i], 'Q');
      if (qdd && *(qdd+1)) {
        QDbarrier = 1;
        qDepth = atoi(qdd+1);
      }
    }

    threadContext[i].QDbarrier = QDbarrier;
    if (qDepth < 1) qDepth = 1;
    if (qDepth > 65535) qDepth = 65535;

    size_t mp = ceil((threadContext[i].maxbdSize - threadContext[i].minbdSize) * 1.0 / bs);

    threadContext[i].firstPPositions = 0;
    char *pChar = strchr(job->strings[i], 'P');
    {
      if (pChar && *(pChar+1)) {
        threadContext[i].firstPPositions = atoi(pChar + 1);
      }
    }

    size_t uniqueSeeds = 0, verifyUnique = 0;
    long metaData = 0;

    {
      // metaData mode is random, verify all writes and flushing, sets QD to 1
      char *iR = strchr(job->strings[i], 'u');
      if (iR) {
        metaData = 1;
        threadContext[i].flushEvery = 1;
        uniqueSeeds = 1;
        //	qDepth = 1;
      }
    }


    {
      // metaData mode is random, verify all writes and flushing, sets QD to 1
      char *iR = strchr(job->strings[i], 'U');
      if (iR) {
        metaData = 1;
        threadContext[i].flushEvery = 1;
        uniqueSeeds = 1;
        qDepth = 1;
        verifyUnique = 1;
      }
    }


    threadContext[i].queueDepth = qDepth;



    if (verbose) {
      fprintf(stderr,"*info* device '%s', size [%.3lf, %.3lf] %.3lf GiB, minsize of %zd, maximum %zd positions\n", job->devices[i], TOGiB(threadContext[i].minbdSize), TOGiB(threadContext[i].maxbdSize), TOGiB(threadContext[i].maxbdSize - threadContext[i].minbdSize), bs, mp);
    }


    // use 1/4 of free RAM
    size_t useRAM = MIN((size_t)15L*1024*1024*1024, freeRAM() / 2) / num; // 1L*1024*1024*1024;
    if (ramBytesForPositions) {
      useRAM = ramBytesForPositions / num;
    }

    size_t aioSize = 2 * threadContext[i].queueDepth * (threadContext[i].highBlockSize); // 2* as read and write

    //    fprintf(stderr,"*info* RAM to use: %zd bytes (%.3lf GiB), qd = %zd, maxK %zd\n", useRAM, TOGiB(useRAM), threadContext[i].queueDepth, threadContext[i].highBlockSize);
    if (aioSize > useRAM / 2) {
      if (i == 0) {
        fprintf(stderr,"*warning* we can't enforce RAM limits with the qd and max block size\n");
      }
    } else {
      useRAM -= aioSize;
    }

    size_t fitinram = (useRAM / sizeof(positionType));
    if (verbose) {
      fprintf(stderr,"*info* RAM required: %.1lf GiB for qd x bs, and %.1lf GiB for positions\n", TOGiB(aioSize), TOGiB(fitinram));
    }



    if ((ramBytesForPositions || verbose || (fitinram < mp)) && (i==0)) {
      fprintf(stderr,"*info* using %.3lf GiB RAM for positions (%d threads, %ld record size), we can store max ", TOGiB(useRAM), num, sizeof(positionType));
      commaPrint0dp(stderr, fitinram);
      fprintf(stderr," positions in RAM\n");
    }


    size_t countintime = mp;
    //        fprintf(stderr,"*info* runX %zd ttr %zd\n", threadContext[i].LBAtimes, threadContext[i].runSeconds);
    if (threadContext[i].runSeconds < 3600) { // only limit based on time if it's interactive, e.g. less than an hour


      int ESTIMATEIOPS=getIOPSestimate(job->devices[i], bs, (i == 0) && verbose);

      countintime = ceil(threadContext[i].runSeconds * ESTIMATEIOPS);
      //      assert (countintime >= 0);
      if ((verbose || (countintime < mp)) && (i == 0)) {
        fprintf(stderr,"*info* in %.2lf seconds, at %d IOPS, would have at most %zd positions\n", threadContext[i].runSeconds, ESTIMATEIOPS, countintime);
      }
    } else {
      if (i==0) fprintf(stderr,"*info* skipping time based limits\n");
    }
    size_t sizeLimitCount = (size_t)-1;
    if (limit != (size_t)-1) {
      sizeLimitCount = limit / bs;
      if (i == 0) {
        fprintf(stderr,"*info* to limit sum of lengths to %.1lf GiB, with avg size of %zd, requires %zd positions\n", TOGiB(limit), bs, sizeLimitCount);
      }
    }
    if (verbose) {
      fprintf(stderr,"*info* mp %zd, sizeLimit %zu, countin time %zd, mp %zd, fitinram %zd\n", mp, limit != (size_t)-1 ? sizeLimitCount : 0, countintime, mp, fitinram);
    }
    

    threadContext[i].jobstring = job->strings[i];
    threadContext[i].jobdevice = job->devices[i];
    threadContext[i].jobdeviceid = job->deviceid[i];
    threadContext[i].jobnuma = job->suggestedNUMA[i];
    threadContext[i].waitfor = 0;
    threadContext[i].exec = (job->delay[i] != 0);
    threadContext[i].prewait = job->delay[i];
    threadContext[i].rerandomize = 0;
    threadContext[i].addBlockSize = 0;

    // do this here to allow repeatable random numbers
    int rcount = 0, wcount = 0, rwt_total = 0, tcount = 0;
    double rprob = 0, wprob = 0, tprob = 0;

    for (size_t k = 0; k < strlen(job->strings[i]); k++) {
      if (job->strings[i][k] == 'r') {
        rcount++;
        rwt_total++;
      } else if (job->strings[i][k] == 'w') {
        wcount++;
        rwt_total++;
      } else if (job->strings[i][k] == 't') {
        tcount++;
        rwt_total++;
      }
    }
    if (rwt_total == 0) {
      rprob = 0.5;
      wprob = 0.5;
    }  else {
      rprob = rcount * 1.0 / rwt_total;
      wprob = wcount * 1.0 / rwt_total;
      tprob = tcount * 1.0 / rwt_total;
    }


    {
      char *sf = strchr(job->strings[i], 'p');
      if (sf && *(sf+1)) {
        rprob = atof(sf + 1);
        wprob = 1.0 - rprob;
        tprob = 0;
      }
    }

    threadContext[i].rw.rprob = rprob;
    threadContext[i].rw.wprob = wprob;
    threadContext[i].rw.tprob = tprob;

    if (verbose || ((i==0) && tprob)) {
      fprintf(stderr,"*info* [t%d] setting action probabilities r=%.2g, w=%.2g, t=%.2g\n", i, rprob, wprob, tprob);
    }

    int flushEvery = 0;
    for (size_t k = 0; k < strlen(job->strings[i]); k++) {
      if (job->strings[i][k] == 'F') {
        if (flushEvery == 0) {
          flushEvery = 1;
        } else {
          flushEvery *= 10;
        }
      }
    }
    threadContext[i].flushEvery = flushEvery;



    {
      // metaData mode is random, verify all writes and flushing
      char *iR = strchr(job->strings[i], 'm');
      if (iR) {
	metaData = -1;
	if (*(iR+1)) {
	  double low, high;
	  int ret = splitRange(iR+1, &low, &high);
	  if (ret && (low > 0)) {
	    metaData = low;
	  }
	}
	fprintf(stderr,"*info* metadata value = %ld\n", metaData);
        seqFiles = 0;
        threadContext[i].flushEvery = 1;
      }
    }



    {
      char *sf = strchr(job->strings[i], 's');
      if (sf && *(sf+1)) {
        double low = 0, high = 0;
        int ret = splitRange(sf + 1, &low, &high);

        if (ret == 0) {
        } else if (ret == 1) {
          seqFilesMaxSizeBytes = 0;
        } else {
          seqFilesMaxSizeBytes = ceil(high) * 1024;
        }
        seqFiles = ceil(low);
      }
    }



    {
      char *sf = strchr(job->strings[i], '@');
      if (sf) {
        fprintf(stderr,"*warning* ignoring results for command '%s'\n", job->strings[i]);
        threadContext[i].ignoreResults = 1;
        threadContext[num].allPC[i] = NULL;
      }
    }


    {
      char *iR = strchr(job->strings[i], 'n');
      if (iR) {// && *(iR+1)) {
        threadContext[i].rerandomize = 1;
      }
    }

    {
      char *iR = strchr(job->strings[i], 'N');
      if (iR) {// && *(iR+1)) {
        threadContext[i].addBlockSize = 1;
      }
    }



    char *iTO = strchr(job->strings[i], 'T');
    if (iTO) {
      if ((threadContext[i].LBAtimes != 0) || (threadContext[i].positionLimit != 0)) {
	fprintf(stderr,"*error* mixing T and (x or X) options\n");
	exit(1);
      }
      threadContext[i].LBAtimes = 0;
      threadContext[i].positionLimit = 0;
      threadContext[i].runSeconds = atof(iTO+1);
      threadContext[i].finishSeconds = atof(iTO+1);
      if (verbose) {
        fprintf(stderr,"*info* hard limiting time to execute to %.1lf seconds\n", threadContext[i].runSeconds);
      }
    }



    long startingBlock = -99999;
    if (strchr(job->strings[i], 'z')) {
      startingBlock = 0;
    }

    {
      char *ZChar = strchr(job->strings[i], 'Z');
      if (ZChar && *(ZChar+1)) {
        startingBlock = atoi(ZChar + 1);
      }
    }

    {
      char *RChar = strchr(job->strings[i], 'R');
      if (RChar && *(RChar+1)) {
        seed = atoi(RChar + 1); // if specified
      } else {
        if (i > 0) seed++;
      }
    }
    threadContext[i].seed = seed;


    size_t speedMB = 0;
    threadContext[i].iopsdecrease = 0;
    {
      char *RChar = strchr(job->strings[i], 'S');
      if (RChar && *(RChar+1)) {
	char retch = ':';
	double s1 = 0, s2 = 0;
	splitRangeChar(RChar + 1, &s1, &s2, &retch);
        speedMB = s1;
	if (s2 != s1) {
	  fprintf(stderr,"*info* IOPS target begin at %lf, decrease every %lf sec\n", s1, s2);
	  threadContext[i].iopsdecrease = s2;
	}
      }
    }
    threadContext[i].iopstarget = speedMB;


    char *Wchar = strchr(job->strings[i], 'W');
    if (Wchar && *(Wchar+1)) {

      char *endp = NULL;
      double runfor = strtod(Wchar + 1, &endp);
      if (runfor < 0) runfor = 0;
      double waitfor = runfor;
      if (*endp == '-' || *endp == ':') {
        waitfor = atof(endp+1);
        if (waitfor < 0) waitfor = 0;
      }

      if (verbose) fprintf(stderr,"*info* alternating time parameter W%.1lf:%.1lf\n", runfor, waitfor);
      if (timeperline > runfor / 2) {
        fprintf(stderr,"*warning* recommendation that -s %.2g is used to get the granularity required\n", runfor / 2);
      }

      threadContext[i].runSeconds = runfor;
      threadContext[i].waitfor = waitfor;
    }

    Wchar = strchr(job->strings[i], 'B');
    if (Wchar && *(Wchar+1)) {
      size_t waitfor = atoi(Wchar + 1);
      if (waitfor > runseconds) {
        waitfor = runseconds;
        fprintf(stderr,"*warning* waiting decreased to %zd\n", waitfor);
      }
      threadContext[i].prewait = waitfor;
    }

    //    threadContext[i].pos.maxbdSize = threadContext[i].maxbdSize;

    if (metaData < 0 || metaData > (long)mp) {
      fprintf(stderr,"*info* setting metadata to %zd\n", mp);
      metaData = mp;
    }
    if (metaData) {
      if ((long)qDepth > metaData) {
	if (i == 0) {
	  fprintf(stderr,"*warning* QD must be <= %zd if reading/writing. Setting QD=%zd\n", metaData, metaData);
	}
	qDepth = metaData;
      }
    }

    // now we have got the G_ and P in any order
    if (threadContext[i].firstPPositions) mp = MIN(mp, threadContext[i].firstPPositions); // min of calculated and specified

    mp = MIN(sizeLimitCount, MIN(countintime, MIN(mp, fitinram)));
    
    if (mp <= qDepth) { // check qd isn't too high
      qDepth = mp;
    }

    threadContext[i].queueDepth = qDepth;

    positionContainerSetup(&threadContext[i].pos, mp);
    threadContext[i].seqFiles = seqFiles;
    threadContext[i].seqFilesMaxSizeBytes = seqFilesMaxSizeBytes;
    threadContext[i].rw.rprob = rprob;
    threadContext[i].rw.wprob = wprob;
    threadContext[i].rw.tprob = tprob;
    threadContext[i].startingBlock = startingBlock;
    threadContext[i].mod = mod;
    threadContext[i].remain = remain;
    threadContext[i].metaData = metaData;
    threadContext[i].uniqueSeeds = uniqueSeeds;
    threadContext[i].verifyUnique = verifyUnique;
    threadContext[i].dumpPos = dumpPos;

  } // setup all threads

  size_t waitft = 0;
  for (int i = 0; i < num; i++) {
    if (threadContext[i].exec == 0) {
      waitft++;
    }
  }
  for (int i = 0; i < num + 1; i++) { // plus one as the last one [num] is the timer thread
    threadContext[i].waitForThreads = waitft;
  }




  // use the device and timing info from context[num]
  threadContext[num].pos.diskStats = d;
  threadContext[num].timeperline = timeperline;
  threadContext[num].ignorefirst = ignorefirst;
  threadContext[num].minbdSize = minSizeInBytes;
  threadContext[num].maxbdSize = maxSizeInBytes;
  // get disk stats before starting the other threads

  if (threadContext[num].pos.diskStats) {
    diskStatStart(threadContext[num].pos.diskStats);
  }


  pthread_mutex_t mutex;
  pthread_mutex_init(&mutex, NULL);
  threadContext[num].gomutex = &mutex;
  for (int i = 0; i < num; i++) {
    threadContext[i].gomutex = &mutex;
  }

  pthread_create(&(pt[num]), NULL, runThreadTimer, &(threadContext[num]));
  pthread_setname_np(pt[num], strdup("spit timer"));

  //  for (int i = 0; i < num; i++) {
  //    fprintf(stderr,"%s  %d\n", threadContext[i].jobstring, threadContext[i].

  int do_numa = strcmp( numaBinding, "" ) != 0;
  int numa_count = getNumaCount();
  int** numa_threads = NULL;
  int* numa_thread_counter = NULL;
  listtype numaList;
  if( do_numa ) {
    numa_threads = (int**)malloc( numa_count * sizeof(int*) );

    for( int numa = 0; numa < numa_count; numa++ ) {
      numa_threads[ numa ] = (int*)malloc( cpuCountPerNuma( numa ) * sizeof(int) );
      getThreadIDs( numa, numa_threads[ numa ] );
    }

    numa_thread_counter = (int*)malloc( numa_count * sizeof( int ) );
    memset( numa_thread_counter, 0, numa_count * sizeof( int ) );

    listConstruct(&numaList);
    if( strcmp(numaBinding, "all") == 0 ) {
      for( int i = 0; i < numa_count; i++ ) {
        listAdd(&numaList, i);
      }
    } else {
      listAddString(&numaList, numaBinding);
    }
    listIterateStart(&numaList);
  } else {
    fprintf( stderr, "*info* NUMA binding disabled\n" );
  }

  for (int tid = 0; tid < num; tid++) {
    char s[100];
    sprintf(s,"spit-t%d", tid);
    pthread_create(&(pt[tid]), NULL, runThread, &(threadContext[tid]));
    pthread_setname_np(pt[tid], strdup(s));

    if( do_numa ) {

      long numa = 0;
      listNext(&numaList, &numa, 1);
      assert( numa >= 0 );
      assert( numa < numa_count );

      // If -I drive.txt, each thread may have a suggest numa already
      if (threadContext[tid].jobnuma >= 0) {
        numa = threadContext[tid].jobnuma;
      } else {
        threadContext[tid].jobnuma = numa;
      }

      ++numa_thread_counter[ numa ];
      int rc = pinThread( &pt[tid], numa_threads[ numa ], cpuCountPerNuma( numa ) );
      if (rc) {
        fprintf(stderr,"*error* failed to pin thread %d to NUMA %ld\n", tid, numa);
        exit(-1);
      }

      if (verbose >= 2) {
        fprintf( stderr, "*info* pinned thread %d to NUMA %ld\n", tid, numa);
      }
    }
  }

  if(verbose >=2 ) {
    fprintf(stderr, "TID\tNUMA\n");
    for(int tid = 0; tid < num; tid++) {
      fprintf(stderr, "%d\t%d\n", tid, threadContext[tid].jobnuma );
    }
  }

  if( do_numa ) {
    int verify = 0;
    fprintf( stderr, "*info* NUMA allocation for %d threads {", num);
    for( int numa = 0; numa < numa_count; numa++ ) {
      const char* delim = numa > 0 ? ", " : "";
      fprintf( stderr, "%s%d", delim, numa_thread_counter[ numa ]);
      verify += numa_thread_counter[numa]; // check all threads are allocated
      free( numa_threads[ numa ] );
    }
    fprintf( stderr, "}\n" );
    free( numa_threads );
    free( numa_thread_counter );
    assert(verify == num);
  }

  // wait for all threads
  for (int i = 0; i < num; i++) {
    pthread_join(pt[i], NULL);
    lengthsFree(&threadContext[i].len);
  }
  keepRunning = 0; // the
  // now wait for the timer thread (probably don't need this)
  pthread_join(pt[num], NULL);

  if (result) {
    result->writeIOPS = threadContext[num].result_writeIOPS;
    result->readIOPS = threadContext[num].result_readIOPS;
    result->writeMBps = threadContext[num].result_writeMBps;
    result->readMBps = threadContext[num].result_readMBps;
  }


  positionContainer *origpc;
  CALLOC(origpc, num, sizeof(positionContainer));
  for (int i = 0; i < num; i++) {
    origpc[i] = threadContext[i].pos;
  }
  
  {
    latencyType lat;
    latencyClear(&lat);
    for (int i = 0; i < num; i++) {
      latencySetup(&lat, &origpc[i]);
    }
    latencyStats(&lat);
    latencyWriteGnuplot(&lat);
    latencyReadGnuplot(&lat);
    latencyFree(&lat);

    latencyLenVsLatency(origpc, num);

  }
  
  positionContainer mergedpc = positionContainerMerge(origpc, num);

  if (savePositions && (savePositions != stdout)) {
    positionContainerSave(&mergedpc, savePositions, mergedpc.maxbdSize, 0, job);
    latencyOverTime(&mergedpc);
    fclose(savePositions);
  }
  
  if (verify) {
    positionContainerCheckOverlap(&mergedpc);
    size_t direct = O_DIRECT;
    for (int i = 0; i < num; i++) { // check all threads, if any aren't direct
      if (threadContext[i].o_direct == 0) {
	direct = 0;
      }
    }
    int errors = verifyPositions(&mergedpc, MIN(256, mergedpc.sz), job, direct, 1 /* sorted */, threadContext->runSeconds, NULL, NULL, NULL, 0, 1, 0);
    if (errors) {
      exit(1);
    }
  }
  
  positionContainerFree(&mergedpc);
  free(origpc);

  // free
  for (int i = 0; i < num; i++) {
    positionContainerFree(&threadContext[i].pos);
    free(threadContext[i].randomBuffer);
  }



  free(go);
  free(allThreadsPC);
  free(threadContext);
  free(pt);
}


void jobAddDeviceToAll(jobType *job, const char *device)
{
  for (int i = 0; i < job->count; i++) {
    job->devices[i] = strdup(device);
  }
}

size_t jobCount(jobType *job)
{
  return job->count;
}

size_t jobRunPreconditions(jobType *preconditions, const size_t count, const size_t minSizeBytes, const size_t maxSizeBytes)
{
  if (count) {
    size_t gSize = alignedNumber(maxSizeBytes, 4096);
    size_t coverage = 2;
    size_t jumble = 0;
    size_t fragmentLBA = 0;

    for (int i = 0; i < (int) count; i++) {
      fprintf(stderr,"*info* precondition %d: device '%s', command '%s'\n", i+1, preconditions->devices[i], preconditions->strings[i]);
      fprintf(stderr,"*info* size of the block device %zd (%.3lf GiB)\n", maxSizeBytes, TOGiB(maxSizeBytes));

      
      { //'f' for fragment LBA. f10 is 10% overhead
        char *charG = strchr(preconditions->strings[i], 'f');
        if (charG && (*(charG+1)>='0' && *(charG+1)<='9')) {
	  fragmentLBA = (int)(100.0 / atof(charG+1) + 0.5);
	}
      }

      {
        char *charG = strchr(preconditions->strings[i], 'G');
        if (charG && *(charG+1)) {
          // a G num is specified
          gSize = alignedNumber(1024L * atof(charG + 1) * 1024 * 1024, 4096);
          fprintf(stderr,"*info* specified G value %zd (%.3lf GiB)\n", gSize, TOGiB(gSize));
        }
      }

      double blockSize = 1024; // default to 1M
      {
        char *charG = strchr(preconditions->strings[i], 'k');
        if (charG && *(charG+1)) {
          // a k value is specified
          blockSize = atof(charG + 1);
        }
      }

      {
        char *charG = strchr(preconditions->strings[i], 'M');
        if (charG && *(charG+1)) {
          // a k value is specified
          blockSize = atof(charG + 1) * 1024;
        }
      }

      size_t seqFiles = 1;
      {
        // seq or random
        char *charG = strchr(preconditions->strings[i], 's');
        if (charG && *(charG+1)) {
          seqFiles = atoi(charG + 1);
        }
      }
      if (seqFiles != 0) {
        jumble = 1;
      }

      size_t exitIOPS = MAX(200 * 1024 / blockSize, 10); // 200MB/s lowerbound means far too slow, the speed of one drive
      {
        // seq or random
        char *charG = strchr(preconditions->strings[i], 'I');
        if (charG && *(charG+1)) {
          exitIOPS = atoi(charG + 1);
        }
      }

      fprintf(stderr,"*info* coverage %zd, with an IOPS limit of %zd (%.0lf MiB/sec) after %.3lf GiB is written\n", coverage, exitIOPS, exitIOPS * blockSize / 1024, TOGiB(gSize));

      char s[100];
      sprintf(s, "w k%g z s%zd J%zd b%zd X%zd nN I%zd q64", blockSize, seqFiles, jumble, gSize, coverage, exitIOPS); // X1 not x1 means run once then rerandomise
      fprintf(stderr,"*info* '%s'\n", s);
      free(preconditions->strings[i]);
      preconditions->strings[i] = strdup(s);
    }
    if (fragmentLBA)  {
      const size_t stepgb = 64; 
      fprintf(stderr,"*info* precondition: step through LBA=%zd positions, LBA=[%.2lf, %.2lf) GB, batch %zd GB\n", fragmentLBA, TOGB(minSizeBytes), TOGB(maxSizeBytes), stepgb);
      for (size_t p = TOGiB(minSizeBytes); p < TOGiB(maxSizeBytes); p+= stepgb) if (keepRunning) {
	free(preconditions->strings[0]); // free 'f'
	char s[100];
	sprintf(s,"wx1zs1k4G%zd-%.1lf", p, MIN(TOGiB(maxSizeBytes), p+stepgb));
	fprintf(stderr,"*info* precondition: running string %s\n", s);
	preconditions->strings[0] = strdup(s);
	jobRunThreads(preconditions, 1, NULL, p * 1024L * 1024 * 1024, MIN(maxSizeBytes, (p+stepgb) * 1024L * 1024L * 1024), -1, 0, NULL, 128, 0 /*seed*/, 0 /*save positions*/, NULL, 1, 0, 0 /*noverify*/, NULL, NULL, NULL, "all" /* NUMA */, 0 /* TRIM */, NULL /* results*/, 0, 0);

	free(preconditions->strings[0]); // free 'f'
	sprintf(s,"wx1zs1k4G%zd-%.1lfK%zd", p, MIN(TOGiB(maxSizeBytes), p+stepgb), fragmentLBA);
	fprintf(stderr,"*info* precondition: running string %s\n", s);
	preconditions->strings[0] = strdup(s);
	jobRunThreads(preconditions, 1, NULL, p * 1024L * 1024 * 1024, MIN(maxSizeBytes, (p+stepgb) * 1024L * 1024L * 1024), -1, 0, NULL, 128, 0 /*seed*/, 0 /*save positions*/, NULL, 1, 0, 0 /*noverify*/, NULL, NULL, NULL, "all" /* NUMA */, 0 /* TRIM */, NULL /* results*/, 0, 0);

	
	}
    } else {
      jobRunThreads(preconditions, count, NULL, 0 * minSizeBytes, gSize, -1, 0, NULL, 128, 0 /*seed*/, 0 /*save positions*/, NULL, 1, 0, 0 /*noverify*/, NULL, NULL, NULL, "all" /* NUMA */, 0 /* TRIM */, NULL /* results*/, 0, 0);
    }
    if (keepRunning) {
      fprintf(stderr,"*info* preconditioning complete... waiting for 10 seconds for I/O to stop...\n");
      sleep(10);
      fflush(stderr);
    }
  }
  return 0;
}








