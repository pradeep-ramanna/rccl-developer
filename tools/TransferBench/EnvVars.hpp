#ifndef ENVVARS_HPP
#define ENVVARS_HPP

// This class manages environment variable that affect TransferBench
class EnvVars
{
public:
  // Default configuration values
  int const DEFAULT_NUM_WARMUPS      =  3;
  int const DEFAULT_NUM_ITERATIONS   = 10;
  int const DEFAULT_SAMPLING_FACTOR  =  1;
  int const DEFAULT_NUM_CPU_PER_LINK =  4;

  // Environment variables
  int useHipCall;      // Use hipMemcpy/hipMemset instead of custom shader kernels
  int useMemset;       // Perform a memset instead of a copy (ignores source memory)
  int useSingleSync;   // Perform synchronization only once after all iterations instead of per iteration
  int useInteractive;  // Pause for user-input before starting transfer loop
  int useSleep;        // Adds a 100ms sleep after each synchronization
  int combineTiming;   // Combines the timing with kernel launch
  int showAddr;        // Print out memory addresses for each Link
  int outputToCsv;     // Output in CSV format
  int byteOffset;      // Byte-offset for memory allocations
  int numWarmups;      // Number of un-timed warmup iterations to perform
  int numIterations;   // Number of timed iterations to perform
  int samplingFactor;  // Affects how many different values of N are generated (when N set to 0)
  int numCpuPerLink;   // Number of CPU child threads to use per CPU link
  std::vector<float> fillPattern; // Pattern of floats used to fill source data

  // Constructor that collects values
  EnvVars()
  {
    useHipCall      = GetEnvVar("USE_HIP_CALL"     , 0);
    useMemset       = GetEnvVar("USE_MEMSET"       , 0);
    useSingleSync   = GetEnvVar("USE_SINGLE_SYNC"  , 0);
    useInteractive  = GetEnvVar("USE_INTERACTIVE"  , 0);
    combineTiming   = GetEnvVar("COMBINE_TIMING"   , 0);
    showAddr        = GetEnvVar("SHOW_ADDR"        , 0);
    outputToCsv     = GetEnvVar("OUTPUT_TO_CSV"    , 0);
    byteOffset      = GetEnvVar("BYTE_OFFSET"      , 0);
    numWarmups      = GetEnvVar("NUM_WARMUPS"      , DEFAULT_NUM_WARMUPS);
    numIterations   = GetEnvVar("NUM_ITERATIONS"   , DEFAULT_NUM_ITERATIONS);
    samplingFactor  = GetEnvVar("SAMPLING_FACTOR"  , DEFAULT_SAMPLING_FACTOR);
    numCpuPerLink   = GetEnvVar("NUM_CPU_PER_LINK" , DEFAULT_NUM_CPU_PER_LINK);

    // Check for fill pattern
    char* pattern = getenv("FILL_PATTERN");
    if (pattern != NULL)
    {
      int patternLen = strlen(pattern);
      if (patternLen % 2)
      {
        printf("[ERROR] FILL_PATTERN must contain an even-number of hex digits\n");
        exit(1);
      }

      // Figure out how many copies of the pattern are necessary to fill a 4-byte float properly
      int copies;
      switch (patternLen % 8)
      {
      case 0:  copies = 1; break;
      case 4:  copies = 2; break;
      default: copies = 4; break;
      }

      fillPattern.resize(copies * patternLen / 8);
      unsigned char* rawData = (unsigned char*) fillPattern.data();

      unsigned char val = 0;
      for (int c = 0; c < copies; c++)
      {
        for (int i = 0; i < patternLen; i++)
        {
          if ('0' <= pattern[i] && pattern[i] <= '9')
            val += (pattern[i] - '0');
          else if ('A' <= pattern[i] && pattern[i] <= 'F')
            val += (pattern[i] - 'A' + 10);
          else if ('a' <= pattern[i] && pattern[i] <= 'f')
            val += (pattern[i] - 'a' + 10);
          else
          {
            printf("[ERROR] FILL_PATTERN must contain an even-number of hex digits (0-9'/a-f/A-F).  (not %c)\n", pattern[i]);
            exit(1);
          }

          // Bit shift or else add and reset to 0
          if (i % 2 == 0)
            val <<= 4;
          else
          {
            rawData[(c * patternLen + i) / 2] = val;
            val = 0;
          }
        }
      }
    }
    else fillPattern.clear();

    // Perform some basic validation
    if (byteOffset % sizeof(float))
    {
      printf("[ERROR] BYTE_OFFSET must be set to multiple of %lu\n", sizeof(float));
      exit(1);
    }
    if (numWarmups < 0)
    {
      printf("[ERROR] NUM_WARMUPS must be set to a non-negative number\n");
      exit(1);
    }
    if (numIterations <= 0)
    {
      printf("[ERROR] NUM_ITERATIONS must be set to a positive number\n");
      exit(1);
    }
    if (samplingFactor < 1)
    {
      printf("[ERROR] SAMPLING_FACTOR must be greater or equal to 1\n");
      exit(1);
    }
    if (numCpuPerLink < 1)
    {
      printf("[ERROR] NUM_CPU_PER_LINK must be greater or equal to 1\n");
      exit(1);
    }
  }

  // Display info on the env vars that can be used
  static void DisplayUsage()
  {
    printf("Environment variables:\n");
    printf("======================\n");
    printf(" USE_HIP_CALL       - Use hipMemcpy/hipMemset instead of custom shader kernels for GPU-executed copies\n");
    printf(" USE_MEMSET         - Perform a memset instead of a copy (ignores source memory)\n");
    printf(" USE_SINGLE_SYNC    - Perform synchronization only once after all iterations instead of per iteration\n");
    printf(" USE_INTERACTIVE    - Pause for user-input before starting transfer loop\n");
    printf(" COMBINE_TIMING     - Combines timing with launch (potentially lower timing overhead)\n");
    printf(" SHOW_ADDR          - Print out memory addresses for each Link\n");
    printf(" OUTPUT_TO_CSV      - Outputs to CSV format if set\n");
    printf(" BYTE_OFFSET        - Initial byte-offset for memory allocations.  Must be multiple of 4. Defaults to 0\n");
    printf(" NUM_WARMUPS=W      - Perform W untimed warmup iteration(s) per test\n");
    printf(" NUM_ITERATIONS=I   - Perform I timed iteration(s) per test\n");
    printf(" SAMPLING_FACTOR=F  - Add F samples (when possible) between powers of 2 when auto-generating data sizes\n");
    printf(" NUM_CPU_PER_LINK=C - Use C threads per Link for CPU-executed copies\n");
    printf(" FILL_PATTERN=STR   - Fill input buffer with pattern specified in hex digits (0-9,a-f,A-F).  Must be even number of digits\n");
  }

  // Display env var settings
  void DisplayEnvVars() const
  {
    if (!outputToCsv)
    {
      printf("Run configuration\n");
      printf("=====================================================\n");
      printf("%-20s = %12d : Using %s for GPU-executed copies\n", "USE_HIP_CALL", useHipCall,
             useHipCall ? "HIP functions" : "custom kernels");
      printf("%-20s = %12d : Performing %s\n", "USE_MEMSET", useMemset,
             useMemset ? "memset" : "memcopy");
      if (useHipCall && !useMemset)
      {
        char* env = getenv("HSA_ENABLE_SDMA");
        printf("%-20s = %12s : %s\n", "HSA_ENABLE_SDMA", env,
               (env && !strcmp(env, "0")) ? "Using blit kernels for hipMemcpy" : "Using DMA copy engines");
      }
      printf("%-20s = %12d : %s\n", "USE_SINGLE_SYNC", useSingleSync,
             useSingleSync ? "Synchronizing only once, after all iterations" : "Synchronizing per iteration");
      printf("%-20s = %12d : Running in %s mode\n", "USE_INTERACTIVE", useInteractive,
             useInteractive ? "interactive" : "non-interactive");
      printf("%-20s = %12d : %s\n", "COMBINE_TIMING", combineTiming,
             combineTiming ? "Using combined timing+launch" : "Using separate timing / launch");
      printf("%-20s = %12d : %s\n", "SHOW_ADDR", showAddr,
             showAddr ? "Displaying src/dst mem addresses" : "Not displaying src/dst mem addresses");
      printf("%-20s = %12d : Output to %s\n", "OUTPUT_TO_CSV", outputToCsv,
             outputToCsv ? "CSV" : "console");
      printf("%-20s = %12d : Using byte offset of %d\n", "BYTE_OFFSET", byteOffset, byteOffset);
      printf("%-20s = %12d : Running %d warmup iteration(s) per topology\n", "NUM_WARMUPS", numWarmups, numWarmups);
      printf("%-20s = %12d : Running %d timed iteration(s) per topology\n", "NUM_ITERATIONS", numIterations, numIterations);
      printf("%-20s = %12d : Using %d CPU thread(s) per CPU-based-copy Link\n", "NUM_CPU_PER_LINK", numCpuPerLink, numCpuPerLink);
      printf("%-20s = %12s : ", "FILL_PATTERN", getenv("FILL_PATTERN") ? "(specified)" : "(unspecified)");
      if (fillPattern.size())
      {
        printf("Pattern: %s", getenv("FILL_PATTERN"));
      }
      else
      {
        printf("Pseudo-random: (Element i = i modulo 383 + 31)");
      }
      printf("\n");
    }
  };

private:
  // Helper function that gets parses environment variable or sets to default value
  int GetEnvVar(std::string const varname, int defaultValue)
  {
    if (getenv(varname.c_str()))
      return atoi(getenv(varname.c_str()));
    return defaultValue;
  }
};

#endif
