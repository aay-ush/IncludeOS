#include <os>
#include <cassert>
const char* service_name__ = "...";

#include <ide>
#include <fs/disk.hpp>
std::shared_ptr<fs::Disk> disk;

void list_partitions(decltype(disk));

void Service::start()
{
  // instantiate memdisk with FAT filesystem
  auto& device = hw::Dev::disk<1, VirtioBlk>();
  disk = std::make_shared<fs::Disk> (device);
  assert(disk);

  // if the disk is empty, we can't mount a filesystem anyways
  if (disk->empty()) panic("Oops! The disk is empty!\n");
  
  // 1. create alot of separate jobs
  for (int i = 0; i < 256; i++)
  device.read(0,
  [i] (fs::buffer_t buffer)
  {
    printf("buffer %d is not null: %d\n", i, !!buffer);
    assert(buffer);
  });
  // 2. create alot of sequential jobs of 1024 sectors each
  // note: if we queue more than this we will run out of RAM
  for (int i = 0; i < 64; i++)
  device.read(0, 1024,
  [i] (fs::buffer_t buffer)
  {
    printf("buffer %d is not null: %d\n", i, !!buffer);
    assert(buffer);
  });
  //return;
  
  // list extended partitions
  list_partitions(disk);

  // mount first valid partition (auto-detect and mount)
  disk->mount(
              [] (fs::error_t err)
              {
                if (err)
                  {
                    printf("Could not mount filesystem\n");
                    return;
                  }

                // async ls
                disk->fs().ls("/",
                              [] (fs::error_t err, auto ents)
                              {
                                if (err)
                                  {
                                    printf("Could not list '/' directory\n");
                                    return;
                                  }

                                // go through directory entries
                                for (auto& e : *ents)
                                  {
                                    printf("%s: %s\t of size %llu bytes (CL: %llu)\n",
                                           e.type_string().c_str(), e.name().c_str(), e.size, e.block);

                                    if (e.is_file())
                                      {
                                        printf("*** Attempting to read: %s\n", e.name().c_str());
                                        disk->fs().read(e, 0, e.size,
                                                        [e] (fs::error_t err, fs::buffer_t buffer, size_t len)
                                                        {
                                                          if (err)
                                                            {
                                                              printf("Failed to read file %s!\n",
                                                                     e.name().c_str());
                                                              return;
                                                            }

                                                          std::string contents((const char*) buffer.get(), len);
                                                          printf("[%s contents]:\n%s\nEOF\n\n",
                                                                 e.name().c_str(), contents.c_str());
                                                        });
                                      }
                                  }
                              });

              }); // disk->auto_detect()

  printf("*** TEST SERVICE STARTED *** \n");
}

void list_partitions(decltype(disk) disk)
{
  disk->partitions(
                   [] (fs::error_t err, auto& parts)
                   {
                     if (err)
                       {
                         printf("Failed to retrieve volumes on disk\n");
                         return;
                       }

                     for (auto& part : parts)
                       printf("[Partition]  '%s' at LBA %u\n",
                              part.name().c_str(), part.lba());
                   });
}
