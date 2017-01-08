#include "cpumonitor.h"

int
_cpumonitor_proc_getcores(void)
{
   char buf[4096], *tok;
   FILE *f;
   int cores = 0, i = 0;

   f = fopen("/proc/stat", "r");
   if (f)
     {
        while (fgets(buf, sizeof(buf), f))
          {
             if (i > 0)
               {
                  tok = strtok(buf, " ");
                  if (!strncmp(tok, "cpu", 3))
                    cores++;
                  else
                    break;
               }
             i++;
          }
        fclose (f);
     }
   return cores;
}

void
_cpumonitor_proc_getusage(Instance *inst)
{
   long total = 0, idle = 0, use, total_change, idle_change;
   int percent = 0, i = 0, j = 0, k = 0;
   char buf[4096], *line, *tok;
   FILE *f;
   CPU_Core *core;

   f = fopen("/proc/stat", "r");
   if (f)
     {
        while (fgets(buf, sizeof(buf), f))
          {
             if (k > 0)
               {
                  total = 0;
                  percent = 0;
                  if (!strncmp(buf, "cpu", 3))
                    {
                       line = strchr(buf, ' ');
                       tok = strtok(line, " ");
                       while (tok)
                         {
                            use = atol(tok);
                            total += use;
                            i++;
                            if (i == 4)
                              idle = use;
                            tok = strtok(NULL, " ");
                         }
                    }
                  else break;
                  core = eina_list_nth(inst->cfg->cpumonitor.cores, j);
                  total_change = total - core->total;
                  idle_change = idle - core->idle;
                  if (total_change != 0)
                    percent = 100 * (1 - ((float)idle_change / (float)total_change));
                  if (percent > 100) percent = 100;
                  else if (percent < 0) percent = 0;
                  core->percent = percent;
                  core->total = total;
                  core->idle = idle;
                  j++;
               }
             k++;
          }
        fclose(f);
     }
}

