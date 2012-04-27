#ifndef JOINABLE_IMPL
#define JOINABLE_IMPL

#include "joinable_types.h"
#include "types/MediaObjectImpl.h"

#include <kms-core.h>

using ::com::kurento::kms::api::Joinable;
using ::com::kurento::kms::api::StreamType;
using ::com::kurento::kms::api::Direction;
using ::com::kurento::kms::api::MediaSession;

namespace com { namespace kurento { namespace kms {

class JoinableImpl : public Joinable, public virtual MediaObjectImpl {
public:
	JoinableImpl(MediaSession &session);
	~JoinableImpl() throw();

	void getStreams(std::vector<StreamType::type> &_return);

	void join(const JoinableImpl& to, const Direction direction);
	void unjoin(JoinableImpl& to);

	void join(JoinableImpl &to, const StreamType::type stream, const Direction direction);
	void unjoin(JoinableImpl &to, const StreamType::type stream);

	void getJoinees(std::vector<Joinable> &_return);
	void getJoinees(std::vector<Joinable> &_return, const Direction direction);

	void getJoinees(std::vector<Joinable> &_return, const StreamType::type stream);
	void getJoinees(std::vector<Joinable> &_return, const StreamType::type stream, const Direction direction);

protected:

	KmsEndpoint *endpoint = NULL;
	std::map<JoinableImpl *, KmsLocalConnection *> joinees;

private:

	KmsConnection &create_local_connection();
};

}}} // com::kurento::kms

#endif /* JOINABLE_IMPL */
