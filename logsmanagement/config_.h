/** @file config_.h
 *  @brief Configuration settings for the Netdata Logs service
 *
 *  @author Dimitris Pantazis
 */

#ifndef CONFIG__H_
#define CONFIG__H_

#define KiB * 1024UL
#define MiB * 1048576UL
#define GiB * 1073741824UL

#define DATABASES_DIR "databases/" /**< Path to store the collected logs and their metadata in */
#define MAX_LOG_MSG_SIZE 50 MiB   /**< Maximum allowable log message size (in Bytes) to be stored in message queue and DB. **/
#define DB_FLUSH_BUFF_INTERVAL 8000U /**< Interval (in ms) to attempt to flush individual queues to DB. **/
#define LOG_FILE_READ_INTERVAL 1000U /**< Minimum interval (in ms) to permit reading of log file contents in message queue. **/
#define CIRCULAR_BUFF_SIZE (16U)     /**< IMPORTANT!: Needs to always be a power of 2! Also, note that true size is CIRCULAR_BUFF_SIZE - 1 because 1 element is empty when buff is full **/
#define VALIDATE_COMPRESSION 0 /**< For testing purposes only as it slows down compression considerably. **/
#define MAX_FILE_SIGNATURE_SIZE 1 KiB /**< Maximum signature size to uniquely identify a file. It is used to detect log rotations. */
#define FS_EVENTS_REENABLE_INTERVAL 1000U /**< Interval to wait for before attempting to re-register a certain log file, after it was not found (due to rotation or other reason). **/
#define BUFF_SCALE_FACTOR 1.5             /**< Scaling of buffers where reallocating memory (where applicable). **/

#define BLOB_MAX_SIZE 200 MiB /**< Maximum quota for BLOB files, used to store compressed logs. When exceeded, the BLOB file will be rotated. **/
#define BLOB_MAX_FILES 10	  /**< Maximum allowed number of BLOB files (per collection) that are used to store compressed logs. When exceeded, the olderst one will be overwritten. **/
#define MAX_PATH_LENGTH 4096  /**< Max path length - required for some static allocations **/

#endif  // CONFIG__H_
