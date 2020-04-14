
#ifndef _ASYNC_GENERATOR_H_
#define _ASYNC_GENERATOR_H_

#if defined(SERIAL) && SERIAL
#define ASYNC_GEN_SERIAL 1
#define ASYNC_GEN_PARALLEL 0
#else 
#define ASYNC_GEN_SERIAL 0
#define ASYNC_GEN_PARALLEL 1
#endif

#if ASYNC_GEN_PARALLEL
#include "AsyncObjectStream.hpp"
#endif

#include <functional>
#include <queue>

#include <sstream>


namespace AsyncGeneratorDetails {

#if ASYNC_GEN_PARALLEL
	// static int maxThreads = 4; 
	static int maxThreads = 1000; // std::thread::hardware_concurrency();
	static std::atomic<int> currentThreads;
#else 
	static int maxThreads = 0; 
	static int currentThreads = 0;
#endif
}

/**
 * A class to encapsulate the asynchronous generation of objects from some
 * function. The general scheme is that some "producer" function will 
 * add results to this generator via the generateObject method.
 * Then, the consumer will retrieve objects that were generated via the getNextObject
 * method. 
 *
 * Note1: All functions passed to the generator as "producer" functions should take, 
 * as their final parameter, an AsyncGenerator reference. This allows the producer
 * to obtain a shared instance of the AsyncGenerator and use it to generate
 * the objects. This AsyncGenerator should *not* appear in the last of arguments
 * passed to the AsyncGenerator constructor; it will automatically be inserted.
 * 
 * Note2: this class need not necessarily be asynchronous, say if wishing to build
 * in a serial fashion. In that case the interface remains the same but is the 
 * generation of objects is not done serially. 
 *
 * 
 */

template <class Object>
class AsyncGenerator {

	// int inc_i = 0;
	
#if ASYNC_GEN_PARALLEL
	AsyncObjectStream<Object> stream;
	std::thread m_thread;
	bool executingAsync;
	std::recursive_mutex m_mutex;
#endif

	//queue used either by a SERIAL build or when max threads reached.
	std::queue<Object> results;
	bool finishedProducing;

	
	/**
	 * Join the generation method to the current thread. 
	 * Effectively waits for all possible objects to be generated.
	 */
	void join() {
#if ASYNC_GEN_PARALLEL
		if (m_thread.joinable()) {
			m_thread.join();
		}
#else 
#endif
	}

	/**
	 * Detch the generation method from the current thread.
	 * Allows the generation to occur in a detached way from the main thead.
	 */
	void detach() {
#if ASYNC_GEN_PARALLEL
		if (m_thread.joinable()) {
			m_thread.detach();
		}
#else 
#endif
	}


public:


	//Tricky tricky here. Passing this, which is not fully constructed yet, 
	//to the function. If this class ever gets more complicated, can cause
	//weird errors in order of initialization.
	/**
	 * Construct an AsyncGenerator given some "producer" function and the arguments
	 * to pass to that function.
	 *
	 * All functions passed should take, as their final parameter, 
	 * an AsyncGenerator reference. This allows the producer
	 * to obtain a shared instance of the AsyncGenerator and use it to generate
	 * the objects. An AsyncGenerator reference should not be passed in the list of args.
	 * An AsyncGenerator reference will automatically be inserted.
	 *
	 */
	template <class Function, class... Args>
	explicit AsyncGenerator(Function&& f, Args&&...args) :
#if ASYNC_GEN_PARALLEL
		stream(),
		m_thread(),
		executingAsync(0),
#endif
		finishedProducing(0)
	{
#if ASYNC_GEN_PARALLEL
		if (AsyncGeneratorDetails::currentThreads < AsyncGeneratorDetails::maxThreads) {
			executingAsync = 1;
			++(AsyncGeneratorDetails::currentThreads);
			// fprintf(stderr, "start currentThreads = %d\n", AsyncGeneratorDetails::currentThreads.load());		
			m_thread = std::thread(std::forward<Function>(f), std::forward<Args>(args)..., std::ref(*this));
		} else {
			//perform operation on the current thread as if it was serial.
			// if (memFun) {
			auto boundF = std::bind(f, args..., std::ref(*this));
			boundF();
				// fOwner->*f(args..., std::ref(*this));
			// } else {
				// f(args..., std::ref(*this));				
			// }
		}
#endif
#if ASYNC_GEN_SERIAL
		auto boundF = std::bind(f, args..., std::ref(*this));
		boundF();
		finishedProducing = 1;
#endif
	}


	/**
	 * Create an empty AsyncGenerator that will never produce anything
	 * since it has not producing method.
	 */
	AsyncGenerator() :
#if ASYNC_GEN_PARALLEL
		stream(),
		m_thread(),
		executingAsync(0),
#endif
		finishedProducing(1)
	{

	}

	~AsyncGenerator() {
#if ASYNC_GEN_PARALLEL
		this->setComplete();
		this->detach();
		if(executingAsync) {
			--(AsyncGeneratorDetails::currentThreads);			
		}

		// fprintf(stderr, "end currentThreads = %d\n", AsyncGeneratorDetails::currentThreads.load());

		
#endif
	}

	/**
	 * Add a new object to the generated by this AsyncGenerator.
	 * Note that the passed Object is moved from.
	 *
	 * obj: the Object to generate.
	 */
	void generateObject(Object& obj) {

		// inc_i++;
		// fprintf(stderr, "the number of genObj = %d\n", inc_i);
		// fprintf(stderr, "currentThreads = %d\n", AsyncGeneratorDetails::currentThreads.load());

#if ASYNC_GEN_PARALLEL
		if (executingAsync) {
			stream.addResult(obj);
		} else {
#endif
			results.push(std::move(obj));

#if ASYNC_GEN_PARALLEL			
		}
#endif
	}

	/**
	 * Add a new object to the generated by this AsyncGenerator.
	 * Note that the passed Object is moved from.
	 *
	 * obj: the Object to generate.
	 */
	void generateObject(const Object& obj) {

		// inc_i++;
		// fprintf(stderr, "\n\n the number of genObj = %d\n", inc_i);	
		// fprintf(stderr, "currentThreads = %d\n", AsyncGeneratorDetails::currentThreads.load());

#if ASYNC_GEN_PARALLEL
		if (executingAsync) {
			Object objTemp(obj);
			stream.addResult(objTemp);
		} else {
#endif
			results.push(obj);

#if ASYNC_GEN_PARALLEL			
		}
#endif
	}

	/**
	 * Finalize the AsyncGenerator by the producer declaring it has finished
	 * generating all possible objects.
	 */
	void setComplete() {
#if ASYNC_GEN_PARALLEL
		if (executingAsync) {
			stream.resultsFinished();
		} else {
#endif
			finishedProducing = 1;

#if ASYNC_GEN_PARALLEL
		}
#endif
	}

	/**
	 * Obtain an Object which was generated.
	 * The generated object is returned in obj.
	 * 
	 * returns false iff no more objects could be obtained and
	 * no more objects will ever be generated.
	 */
	bool getNextObject(Object& obj) {
#if ASYNC_GEN_PARALLEL
		if (executingAsync) {
			return stream.getNextObject(obj);
		} else {
#endif
			if(finishedProducing && results.empty()) {
				return false;
			}
			obj = std::move(results.front());
			results.pop();
			return true;
#if ASYNC_GEN_PARALLEL
		}
#endif

	}

	/**
	 * Set the maximum number of threads to be active at one time
	 * from all instances of AsyncGenerator in the application. 
	 */
	static void maxThreads(int newmaxThreads) {
		AsyncGeneratorDetails::maxThreads = newmaxThreads;
	}

};




#endif